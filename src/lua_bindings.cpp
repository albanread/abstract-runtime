#include "lua_bindings.h"
#include "abstract_runtime.h"
#include "input_system.h"
#include "sprite_allocator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <filesystem>

// =============================================================================
// LUAJIT THREAD MANAGER IMPLEMENTATION
// =============================================================================

std::unique_ptr<LuaThreadManager> LuaThreadManager::instance = nullptr;
std::mutex LuaThreadManager::instance_mutex;
std::mutex LuaThreadManager::runtime_api_mutex;

// Static globals for runtime state
static bool g_runtime_initialized = false;
static bool g_runtime_pre_initialized = false;

// LuaThread destructor
LuaThread::~LuaThread() {
    should_stop = true;
    if (thread.joinable()) {
        thread.join();
    }
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

// LuaThreadManager singleton implementation
LuaThreadManager& LuaThreadManager::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex);
    if (!instance) {
        instance = std::unique_ptr<LuaThreadManager>(new LuaThreadManager());
    }
    return *instance;
}

void LuaThreadManager::lock_runtime_api() {
    runtime_api_mutex.lock();
}

void LuaThreadManager::unlock_runtime_api() {
    runtime_api_mutex.unlock();
}

LuaThreadHandle LuaThreadManager::exec_lua(const std::string& filepath) {
    // Check if file exists
    if (!std::filesystem::exists(filepath)) {
        std::cerr << "Lua script file not found: " << filepath << std::endl;
        return nullptr;
    }

    // Read file content
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open Lua script: " << filepath << std::endl;
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script_content = buffer.str();
    file.close();

    // Create thread handle
    auto thread_handle = std::make_shared<LuaThread>(next_thread_id.fetch_add(1));
    thread_handle->filepath = filepath;
    thread_handle->script_content = script_content;

    // Add to thread list
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        threads.push_back(thread_handle);
    }

    // Start the thread
    thread_handle->thread = std::thread(&LuaThreadManager::thread_worker, this, thread_handle);
    
    return thread_handle;
}

LuaThreadHandle LuaThreadManager::exec_lua_string(const std::string& script, const std::string& name) {
    auto thread_handle = std::make_shared<LuaThread>(next_thread_id.fetch_add(1));
    thread_handle->filepath = name;
    thread_handle->script_content = script;

    // Add to thread list
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        threads.push_back(thread_handle);
    }

    // Start the thread
    thread_handle->thread = std::thread(&LuaThreadManager::thread_worker, this, thread_handle);
    
    return thread_handle;
}

bool LuaThreadManager::stop_lua_thread(LuaThreadHandle thread) {
    if (!thread) return false;
    
    thread->should_stop = true;
    if (thread->thread.joinable()) {
        thread->thread.join();
        thread->status = LuaThreadStatus::STOPPED;
        return true;
    }
    return false;
}

void LuaThreadManager::stop_all_threads() {
    std::lock_guard<std::mutex> lock(threads_mutex);
    for (auto& thread : threads) {
        if (thread && thread->status == LuaThreadStatus::RUNNING) {
            thread->should_stop = true;
        }
    }
    
    // Wait for all threads to finish
    for (auto& thread : threads) {
        if (thread && thread->thread.joinable()) {
            thread->thread.join();
        }
    }
}

void LuaThreadManager::cleanup_finished_threads() {
    std::lock_guard<std::mutex> lock(threads_mutex);
    threads.erase(
        std::remove_if(threads.begin(), threads.end(),
            [](const LuaThreadHandle& thread) {
                return !thread || thread->status == LuaThreadStatus::FINISHED || 
                       thread->status == LuaThreadStatus::ERROR ||
                       thread->status == LuaThreadStatus::STOPPED;
            }),
        threads.end()
    );
}

std::vector<LuaThreadHandle> LuaThreadManager::get_active_threads() {
    std::lock_guard<std::mutex> lock(threads_mutex);
    std::vector<LuaThreadHandle> active;
    for (const auto& thread : threads) {
        if (thread && thread->status == LuaThreadStatus::RUNNING) {
            active.push_back(thread);
        }
    }
    return active;
}

std::vector<LuaThreadHandle> LuaThreadManager::get_all_threads() {
    std::lock_guard<std::mutex> lock(threads_mutex);
    return threads;
}

int LuaThreadManager::get_thread_count() const {
    std::lock_guard<std::mutex> lock(threads_mutex);
    int count = 0;
    for (const auto& thread : threads) {
        if (thread && thread->status == LuaThreadStatus::RUNNING) {
            count++;
        }
    }
    return count;
}

void LuaThreadManager::shutdown() {
    shutdown_requested = true;
    stop_all_threads();
    
    std::lock_guard<std::mutex> lock(threads_mutex);
    threads.clear();
}

void LuaThreadManager::thread_worker(LuaThreadHandle thread_handle) {
    if (!thread_handle) return;

    // Create Lua state for this thread
    thread_handle->L = create_thread_lua_state();
    if (!thread_handle->L) {
        thread_handle->status = LuaThreadStatus::ERROR;
        thread_handle->error_message = "Failed to create Lua state";
        return;
    }

    thread_handle->status = LuaThreadStatus::RUNNING;

    try {
        // Execute the script
        int result = luaL_loadbuffer(thread_handle->L, 
                                    thread_handle->script_content.c_str(),
                                    thread_handle->script_content.length(),
                                    thread_handle->filepath.c_str());
        
        if (result != LUA_OK) {
            thread_handle->status = LuaThreadStatus::ERROR;
            thread_handle->error_message = lua_tostring(thread_handle->L, -1);
            return;
        }

        // Execute the loaded script
        result = lua_pcall(thread_handle->L, 0, LUA_MULTRET, 0);
        if (result != LUA_OK) {
            thread_handle->status = LuaThreadStatus::ERROR;
            thread_handle->error_message = lua_tostring(thread_handle->L, -1);
            return;
        }

        thread_handle->status = LuaThreadStatus::FINISHED;
    } catch (const std::exception& e) {
        thread_handle->status = LuaThreadStatus::ERROR;
        thread_handle->error_message = e.what();
    }

    cleanup_lua_state(thread_handle->L);
    thread_handle->L = nullptr;
}

lua_State* LuaThreadManager::create_thread_lua_state() {
    lua_State* L = luaL_newstate();
    if (!L) {
        return nullptr;
    }

    // Open standard libraries
    luaL_openlibs(L);

    // Initialize our runtime bindings
    luaopen_abstract_runtime(L);

    return L;
}

void LuaThreadManager::cleanup_lua_state(lua_State* L) {
    if (L) {
        lua_close(L);
    }
}

// =============================================================================
// PUBLIC API FUNCTIONS
// =============================================================================

void init_lua_thread_manager() {
    // Initialize singleton (will be created on first access)
    LuaThreadManager::getInstance();
}

void shutdown_lua_thread_manager() {
    auto& manager = LuaThreadManager::getInstance();
    manager.shutdown();
}

LuaThreadHandle exec_lua(const std::string& filepath) {
    return LuaThreadManager::getInstance().exec_lua(filepath);
}

LuaThreadHandle exec_lua_string(const std::string& script, const std::string& name) {
    return LuaThreadManager::getInstance().exec_lua_string(script, name);
}

bool stop_lua_thread(LuaThreadHandle thread) {
    return LuaThreadManager::getInstance().stop_lua_thread(thread);
}

std::vector<LuaThreadHandle> get_active_lua_threads() {
    return LuaThreadManager::getInstance().get_active_threads();
}

// =============================================================================
// LUAJIT COMPATIBILITY HELPERS
// =============================================================================

const char* luajit_tolstring(lua_State* L, int idx, size_t* len) {
    switch (lua_type(L, idx)) {
        case LUA_TNUMBER:
        case LUA_TSTRING:
            return lua_tolstring(L, idx, len);
        case LUA_TBOOLEAN:
            lua_pushstring(L, lua_toboolean(L, idx) ? "true" : "false");
            return lua_tolstring(L, -1, len);
        case LUA_TNIL:
            lua_pushstring(L, "nil");
            return lua_tolstring(L, -1, len);
        default:
            lua_pushfstring(L, "%s: %p", luaL_typename(L, idx), lua_topointer(L, idx));
            return lua_tolstring(L, -1, len);
    }
}

void luajit_register_compat_functions(lua_State* L) {
    // Register any LuaJIT-specific compatibility functions here
    // Currently none needed, but this is where they would go
}

// =============================================================================
// ERROR HANDLING AND UTILITY FUNCTIONS
// =============================================================================

int lua_runtime_error(lua_State* L, const char* message) {
    lua_pushstring(L, message);
    return lua_error(L);
}

int check_runtime_initialized(lua_State* L) {
    if (!g_runtime_initialized && !g_runtime_pre_initialized) {
        return lua_runtime_error(L, "Runtime not initialized. Call init_abstract_runtime() first.");
    }
    return 0;
}

int validate_coordinates(lua_State* L, int x, int y, const char* func_name) {
    if (x < 0 || y < 0) {
        std::string error = std::string(func_name) + ": Invalid coordinates (" + 
                           std::to_string(x) + ", " + std::to_string(y) + ")";
        return lua_runtime_error(L, error.c_str());
    }
    return 0;
}

int validate_color(lua_State* L, int r, int g, int b, int a, const char* func_name) {
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0 || a > 255) {
        std::string error = std::string(func_name) + ": Invalid color values (" + 
                           std::to_string(r) + ", " + std::to_string(g) + ", " + 
                           std::to_string(b) + ", " + std::to_string(a) + ")";
        return lua_runtime_error(L, error.c_str());
    }
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - RUNTIME INITIALIZATION
// =============================================================================

int lua_init_abstract_runtime(lua_State* L) {
    int screen_mode = luaL_checkinteger(L, 1);
    
    bool result;
    RUNTIME_API_CALL(result = init_abstract_runtime(screen_mode));
    if (result) {
        g_runtime_initialized = true;
    }
    lua_pushboolean(L, result);
    
    return 1;
}

int lua_shutdown_abstract_runtime(lua_State* L) {
    RUNTIME_API_CALL(shutdown_abstract_runtime());
    g_runtime_initialized = false;
    return 0;
}

int lua_exit_runtime(lua_State* L) {
    RUNTIME_API_CALL(exit_runtime());
    return 0;
}

int lua_shutdown(lua_State* L) {
    RUNTIME_API_CALL(exit_runtime());
    return 0;
}

int lua_should_quit(lua_State* L) {
    bool result;
    RUNTIME_API_CALL(result = should_quit());
    lua_pushboolean(L, result);
    return 1;
}

int lua_disable_auto_quit(lua_State* L) {
    RUNTIME_API_CALL(disable_auto_quit());
    return 0;
}

int lua_enable_auto_quit(lua_State* L) {
    RUNTIME_API_CALL(enable_auto_quit());
    return 0;
}

int lua_run_main_loop(lua_State* L) {
    RUNTIME_API_CALL(run_main_loop());
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - DISPLAY CONTROL
// =============================================================================

int lua_set_background_color(lua_State* L) {
    int r = luaL_checkinteger(L, 1);
    int g = luaL_checkinteger(L, 2);
    int b = luaL_checkinteger(L, 3);
    int ret = validate_color(L, r, g, b, 255, "set_background_color");
    if (ret) return ret;
    
    RUNTIME_API_CALL(set_background_color(r, g, b));
    return 0;
}

int lua_get_screen_width(lua_State* L) {
    int result;
    RUNTIME_API_CALL(result = get_screen_width());
    lua_pushinteger(L, result);
    return 1;
}

int lua_get_screen_height(lua_State* L) {
    int result;
    RUNTIME_API_CALL(result = get_screen_height());
    lua_pushinteger(L, result);
    return 1;
}

// =============================================================================
// LUA BINDING FUNCTIONS - TEXT SYSTEM
// =============================================================================

int lua_print_at(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    
    int ret = validate_coordinates(L, x, y, "print_at");
    if (ret) return ret;
    
    RUNTIME_API_CALL(print_at(x, y, text));
    return 0;
}

int lua_clear_text(lua_State* L) {
    RUNTIME_API_CALL(clear_text());
    return 0;
}

int lua_scroll_text(lua_State* L) {
    int lines = luaL_checkinteger(L, 1);
    RUNTIME_API_CALL(scroll_text(0, lines));
    return 0;
}

int lua_scroll_text_up(lua_State* L) {
    RUNTIME_API_CALL(scroll_text_up());
    return 0;
}

int lua_scroll_text_down(lua_State* L) {
    RUNTIME_API_CALL(scroll_text_down());
    return 0;
}

int lua_set_text_ink(lua_State* L) {
    int r = luaL_checkinteger(L, 1);
    int g = luaL_checkinteger(L, 2);
    int b = luaL_checkinteger(L, 3);
    int ret = validate_color(L, r, g, b, 255, "set_text_ink");
    if (ret) return ret;
    
    RUNTIME_API_CALL(set_text_ink(r, g, b, 255));
    return 0;
}

int lua_set_text_paper(lua_State* L) {
    int r = luaL_checkinteger(L, 1);
    int g = luaL_checkinteger(L, 2);
    int b = luaL_checkinteger(L, 3);
    int ret = validate_color(L, r, g, b, 255, "set_text_paper");
    if (ret) return ret;
    
    RUNTIME_API_CALL(set_text_paper(r, g, b, 255));
    return 0;
}

int lua_poke_text_ink(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int r = luaL_checkinteger(L, 3);
    int g = luaL_checkinteger(L, 4);
    int b = luaL_checkinteger(L, 5);
    int a = luaL_checkinteger(L, 6);
    
    int ret = validate_color(L, r, g, b, a, "poke_text_ink");
    if (ret) return ret;
    
    RUNTIME_API_CALL(poke_text_ink(x, y, r, g, b, a));
    return 0;
}

int lua_poke_text_paper(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int r = luaL_checkinteger(L, 3);
    int g = luaL_checkinteger(L, 4);
    int b = luaL_checkinteger(L, 5);
    int a = luaL_checkinteger(L, 6);
    
    int ret = validate_color(L, r, g, b, a, "poke_text_paper");
    if (ret) return ret;
    
    RUNTIME_API_CALL(poke_text_paper(x, y, r, g, b, a));
    return 0;
}

int lua_fill_text_color(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    int ink_r = luaL_checkinteger(L, 5);
    int ink_g = luaL_checkinteger(L, 6);
    int ink_b = luaL_checkinteger(L, 7);
    int ink_a = luaL_checkinteger(L, 8);
    int paper_r = luaL_checkinteger(L, 9);
    int paper_g = luaL_checkinteger(L, 10);
    int paper_b = luaL_checkinteger(L, 11);
    int paper_a = luaL_checkinteger(L, 12);
    
    int ret = validate_color(L, ink_r, ink_g, ink_b, ink_a, "fill_text_color ink");
    if (ret) return ret;
    ret = validate_color(L, paper_r, paper_g, paper_b, paper_a, "fill_text_color paper");
    if (ret) return ret;
    
    RUNTIME_API_CALL(fill_text_color(x, y, width, height, ink_r, ink_g, ink_b, ink_a, paper_r, paper_g, paper_b, paper_a));
    return 0;
}

int lua_fill_text_ink(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    int r = luaL_checkinteger(L, 5);
    int g = luaL_checkinteger(L, 6);
    int b = luaL_checkinteger(L, 7);
    int a = luaL_checkinteger(L, 8);
    
    int ret = validate_color(L, r, g, b, a, "fill_text_ink");
    if (ret) return ret;
    
    RUNTIME_API_CALL(fill_text_ink(x, y, width, height, r, g, b, a));
    return 0;
}

int lua_fill_text_paper(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int width = luaL_checkinteger(L, 3);
    int height = luaL_checkinteger(L, 4);
    int r = luaL_checkinteger(L, 5);
    int g = luaL_checkinteger(L, 6);
    int b = luaL_checkinteger(L, 7);
    int a = luaL_checkinteger(L, 8);
    
    int ret = validate_color(L, r, g, b, a, "fill_text_paper");
    if (ret) return ret;
    
    RUNTIME_API_CALL(fill_text_paper(x, y, width, height, r, g, b, a));
    return 0;
}

int lua_clear_text_colors(lua_State* L) {
    RUNTIME_API_CALL(clear_text_colors());
    return 0;
}

int lua_save_text(lua_State* L) {
    int slot = luaL_checkinteger(L, 1);
    RUNTIME_API_CALL(save_text(slot));
    return 0;
}

int lua_restore_text(lua_State* L) {
    int slot = luaL_checkinteger(L, 1);
    bool result;
    RUNTIME_API_CALL(result = restore_text(slot));
    lua_pushboolean(L, result);
    return 1;
}

int lua_get_saved_text_count(lua_State* L) {
    int count;
    RUNTIME_API_CALL(count = get_saved_text_count());
    lua_pushinteger(L, count);
    return 1;
}

int lua_clear_saved_text(lua_State* L) {
    RUNTIME_API_CALL(clear_saved_text());
    return 0;
}

int lua_wait_for_render_complete(lua_State* L) {
    RUNTIME_API_CALL(wait_for_render_complete());
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - INPUT SYSTEM
// =============================================================================

int lua_is_key_pressed(lua_State* L) {
    int key = luaL_checkinteger(L, 1);
    bool result;
    RUNTIME_API_CALL(result = is_key_pressed(key));
    lua_pushboolean(L, result);
    return 1;
}

int lua_key_just_pressed(lua_State* L) {
    int key = luaL_checkinteger(L, 1);
    bool result;
    RUNTIME_API_CALL(result = key_just_pressed(key));
    lua_pushboolean(L, result);
    return 1;
}

int lua_key_just_released(lua_State* L) {
    int key = luaL_checkinteger(L, 1);
    bool result;
    RUNTIME_API_CALL(result = key_just_released(key));
    lua_pushboolean(L, result);
    return 1;
}

int lua_get_mouse_x(lua_State* L) {
    int result;
    RUNTIME_API_CALL(result = get_mouse_x());
    lua_pushinteger(L, result);
    return 1;
}

int lua_get_mouse_y(lua_State* L) {
    int result;
    RUNTIME_API_CALL(result = get_mouse_y());
    lua_pushinteger(L, result);
    return 1;
}

int lua_init_input_system(lua_State* L) {
    RUNTIME_API_CALL(init_input_system());
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - GRAPHICS
// =============================================================================

int lua_clear_graphics(lua_State* L) {
    RUNTIME_API_CALL(clear_graphics());
    return 0;
}

int lua_draw_line(lua_State* L) {
    int x1 = luaL_checkinteger(L, 1);
    int y1 = luaL_checkinteger(L, 2);
    int x2 = luaL_checkinteger(L, 3);
    int y2 = luaL_checkinteger(L, 4);
    
    RUNTIME_API_CALL(draw_line(x1, y1, x2, y2));
    return 0;
}

int lua_draw_rect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    
    RUNTIME_API_CALL(draw_rect(x, y, w, h));
    return 0;
}

int lua_draw_circle(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int radius = luaL_checkinteger(L, 3);
    
    RUNTIME_API_CALL(draw_circle(x, y, radius));
    return 0;
}

int lua_fill_rect(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int w = luaL_checkinteger(L, 3);
    int h = luaL_checkinteger(L, 4);
    
    RUNTIME_API_CALL(fill_rect(x, y, w, h));
    return 0;
}

int lua_fill_circle(lua_State* L) {
    int x = luaL_checkinteger(L, 1);
    int y = luaL_checkinteger(L, 2);
    int radius = luaL_checkinteger(L, 3);
    
    RUNTIME_API_CALL(fill_circle(x, y, radius));
    return 0;
}

int lua_set_draw_color(lua_State* L) {
    int r = luaL_checkinteger(L, 1);
    int g = luaL_checkinteger(L, 2);
    int b = luaL_checkinteger(L, 3);
    int a = luaL_optinteger(L, 4, 255);
    
    int ret = validate_color(L, r, g, b, a, "set_draw_color");
    if (ret) return ret;
    
    RUNTIME_API_CALL(set_draw_color(r, g, b, a));
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - SPRITES
// =============================================================================

int lua_init_sprites(lua_State* L) {
    RUNTIME_API_CALL(init_sprites());
    return 0;
}

int lua_load_sprite(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    const char* filename = luaL_checkstring(L, 2);
    
    bool result;
    RUNTIME_API_CALL(result = load_sprite(id, filename));
    lua_pushboolean(L, result);
    return 1;
}

int lua_sprite(lua_State* L) {
    int id = luaL_checkinteger(L, 1);
    int x = luaL_checkinteger(L, 2);
    int y = luaL_checkinteger(L, 3);
    
    RUNTIME_API_CALL(sprite(id, id, x, y));
    return 0;
}

// Sprite Allocator Functions
int lua_allocate_sprite(lua_State* L) {
    // Get current thread ID (use Lua thread pointer as unique identifier)
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    auto sprite_id = allocator.allocateSprite(thread_id);
    
    if (sprite_id != AbstractRuntime::SpriteAllocator::INVALID_SPRITE_ID) {
        lua_pushinteger(L, sprite_id);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int lua_allocate_sprites(lua_State* L) {
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    int count = luaL_checkinteger(L, 1);
    
    if (count <= 0) {
        lua_newtable(L);
        return 1;
    }
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    auto sprites = allocator.allocateSprites(thread_id, static_cast<uint32_t>(count));
    
    // Return as Lua table
    lua_newtable(L);
    for (size_t i = 0; i < sprites.size(); ++i) {
        lua_pushinteger(L, sprites[i]);
        lua_rawseti(L, -2, i + 1); // Lua arrays are 1-indexed
    }
    
    return 1;
}

int lua_allocate_specific_sprite(lua_State* L) {
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    int sprite_id = luaL_checkinteger(L, 1);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    bool success = allocator.allocateSpecificSprite(thread_id, static_cast<uint32_t>(sprite_id));
    
    lua_pushboolean(L, success);
    return 1;
}

int lua_release_sprite(lua_State* L) {
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    int sprite_id = luaL_checkinteger(L, 1);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    bool success = allocator.releaseSprite(static_cast<uint32_t>(sprite_id), thread_id);
    
    lua_pushboolean(L, success);
    return 1;
}

int lua_release_all_sprites(lua_State* L) {
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    uint32_t released_count = allocator.releaseAllSprites(thread_id);
    
    lua_pushinteger(L, released_count);
    return 1;
}

int lua_get_owned_sprites(lua_State* L) {
    uint64_t thread_id = reinterpret_cast<uint64_t>(L);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    auto sprites = allocator.getSpritesOwnedBy(thread_id);
    
    // Return as Lua table
    lua_newtable(L);
    for (size_t i = 0; i < sprites.size(); ++i) {
        lua_pushinteger(L, sprites[i]);
        lua_rawseti(L, -2, i + 1); // Lua arrays are 1-indexed
    }
    
    return 1;
}

int lua_is_sprite_available(lua_State* L) {
    int sprite_id = luaL_checkinteger(L, 1);
    
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    bool available = allocator.isSpriteAvailable(static_cast<uint32_t>(sprite_id));
    
    lua_pushboolean(L, available);
    return 1;
}

int lua_get_sprite_allocator_stats(lua_State* L) {
    auto& allocator = AbstractRuntime::SpriteAllocator::getInstance();
    auto stats = allocator.getStats();
    
    // Return as Lua table
    lua_newtable(L);
    
    lua_pushstring(L, "total_sprites");
    lua_pushinteger(L, stats.total_sprites);
    lua_settable(L, -3);
    
    lua_pushstring(L, "allocated_sprites");
    lua_pushinteger(L, stats.allocated_sprites);
    lua_settable(L, -3);
    
    lua_pushstring(L, "available_sprites");
    lua_pushinteger(L, stats.available_sprites);
    lua_settable(L, -3);
    
    lua_pushstring(L, "active_threads");
    lua_pushinteger(L, stats.active_threads);
    lua_settable(L, -3);
    
    return 1;
}

// Console output functions
int lua_console_print(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    console_print(message);
    return 0;
}

int lua_console_info(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    console_info(message);
    return 0;
}

int lua_console_warning(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    console_warning(message);
    return 0;
}

int lua_console_error(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    console_error(message);
    return 0;
}

int lua_console_debug(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    console_debug(message);
    return 0;
}

int lua_console_section(lua_State* L) {
    const char* section_name = luaL_checkstring(L, 1);
    console_section(section_name);
    return 0;
}

int lua_console_separator(lua_State* L) {
    console_separator();
    return 0;
}

// =============================================================================
// LUA BINDING FUNCTIONS - THREADING
// =============================================================================

int lua_exec_lua_file(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    
    auto thread_handle = exec_lua(std::string(filepath));
    if (thread_handle) {
        lua_pushinteger(L, thread_handle->thread_id);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

int lua_exec_lua_string(lua_State* L) {
    const char* script = luaL_checkstring(L, 1);
    const char* name = luaL_optstring(L, 2, "inline");
    
    auto thread_handle = exec_lua_string(std::string(script), std::string(name));
    if (thread_handle) {
        lua_pushinteger(L, thread_handle->thread_id);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

int lua_stop_lua_thread(lua_State* L) {
    int thread_id = luaL_checkinteger(L, 1);
    
    auto threads = get_active_lua_threads();
    for (auto& thread : threads) {
        if (thread->thread_id == thread_id) {
            bool result = stop_lua_thread(thread);
            lua_pushboolean(L, result);
            return 1;
        }
    }
    
    lua_pushboolean(L, false);
    return 1;
}

int lua_get_thread_count(lua_State* L) {
    int count = get_lua_thread_count();
    lua_pushinteger(L, count);
    return 1;
}

int lua_sleep(lua_State* L) {
    double seconds = luaL_checknumber(L, 1);
    auto duration = std::chrono::duration<double>(seconds);
    std::this_thread::sleep_for(duration);
    return 0;
}

// =============================================================================
// REGISTRATION FUNCTIONS
// =============================================================================

void register_runtime_init_functions(lua_State* L) {
    lua_register(L, "init_abstract_runtime", lua_init_abstract_runtime);
    lua_register(L, "shutdown_abstract_runtime", lua_shutdown_abstract_runtime);
    lua_register(L, "exit_runtime", lua_exit_runtime);
    lua_register(L, "shutdown", lua_shutdown);
    lua_register(L, "should_quit", lua_should_quit);
    lua_register(L, "disable_auto_quit", lua_disable_auto_quit);
    lua_register(L, "enable_auto_quit", lua_enable_auto_quit);
    lua_register(L, "run_main_loop", lua_run_main_loop);
}

void register_display_functions(lua_State* L) {
    lua_register(L, "set_background_color", lua_set_background_color);
    lua_register(L, "get_screen_width", lua_get_screen_width);
    lua_register(L, "get_screen_height", lua_get_screen_height);
}

void register_text_functions(lua_State* L) {
    lua_register(L, "print_at", lua_print_at);
    lua_register(L, "clear_text", lua_clear_text);
    lua_register(L, "scroll_text", lua_scroll_text);
    lua_register(L, "scroll_text_up", lua_scroll_text_up);
    lua_register(L, "scroll_text_down", lua_scroll_text_down);
    lua_register(L, "set_text_ink", lua_set_text_ink);
    lua_register(L, "set_text_paper", lua_set_text_paper);
    lua_register(L, "poke_text_ink", lua_poke_text_ink);
    lua_register(L, "poke_text_paper", lua_poke_text_paper);
    lua_register(L, "fill_text_color", lua_fill_text_color);
    lua_register(L, "fill_text_ink", lua_fill_text_ink);
    lua_register(L, "fill_text_paper", lua_fill_text_paper);
    lua_register(L, "clear_text_colors", lua_clear_text_colors);
    lua_register(L, "save_text", lua_save_text);
    lua_register(L, "restore_text", lua_restore_text);
    lua_register(L, "get_saved_text_count", lua_get_saved_text_count);
    lua_register(L, "clear_saved_text", lua_clear_saved_text);
    lua_register(L, "wait_for_render_complete", lua_wait_for_render_complete);
}

void register_cursor_functions(lua_State* L) {
    // Cursor functions would go here when needed
}

void register_input_functions(lua_State* L) {
    lua_register(L, "is_key_pressed", lua_is_key_pressed);
    lua_register(L, "key_just_pressed", lua_key_just_pressed);
    lua_register(L, "key_just_released", lua_key_just_released);
    lua_register(L, "get_mouse_x", lua_get_mouse_x);
    lua_register(L, "get_mouse_y", lua_get_mouse_y);
    lua_register(L, "init_input_system", lua_init_input_system);
}

void register_graphics_functions(lua_State* L) {
    lua_register(L, "clear_graphics", lua_clear_graphics);
    lua_register(L, "draw_line", lua_draw_line);
    lua_register(L, "draw_rect", lua_draw_rect);
    lua_register(L, "draw_circle", lua_draw_circle);
    lua_register(L, "fill_rect", lua_fill_rect);
    lua_register(L, "fill_circle", lua_fill_circle);
    lua_register(L, "set_draw_color", lua_set_draw_color);
}

void register_sprite_functions(lua_State* L) {
    lua_register(L, "init_sprites", lua_init_sprites);
    lua_register(L, "load_sprite", lua_load_sprite);
    lua_register(L, "sprite", lua_sprite);
    
    // Sprite allocation functions
    lua_register(L, "allocate_sprite", lua_allocate_sprite);
    lua_register(L, "allocate_sprites", lua_allocate_sprites);
    lua_register(L, "allocate_specific_sprite", lua_allocate_specific_sprite);
    lua_register(L, "release_sprite", lua_release_sprite);
    lua_register(L, "release_all_sprites", lua_release_all_sprites);
    lua_register(L, "get_owned_sprites", lua_get_owned_sprites);
    lua_register(L, "is_sprite_available", lua_is_sprite_available);
    lua_register(L, "get_sprite_stats", lua_get_sprite_allocator_stats);
}

void register_console_functions(lua_State* L) {
    lua_register(L, "console_print", lua_console_print);
    lua_register(L, "console_info", lua_console_info);
    lua_register(L, "console_warning", lua_console_warning);
    lua_register(L, "console_error", lua_console_error);
    lua_register(L, "console_debug", lua_console_debug);
    lua_register(L, "console_section", lua_console_section);
    lua_register(L, "console_separator", lua_console_separator);
}

void register_tile_functions(lua_State* L) {
    // Tile functions would go here when needed
}

void register_threading_functions(lua_State* L) {
    lua_register(L, "exec_lua", lua_exec_lua_file);
    lua_register(L, "exec_lua_string", lua_exec_lua_string);
    lua_register(L, "stop_lua_thread", lua_stop_lua_thread);
    lua_register(L, "get_thread_count", lua_get_thread_count);
    lua_register(L, "sleep", lua_sleep);
}

void register_utility_functions(lua_State* L) {
    // Utility functions would go here when needed
}

void register_runtime_constants(lua_State* L) {
    // Key constants
    lua_pushinteger(L, 27); lua_setglobal(L, "KEY_ESCAPE");
    lua_pushinteger(L, 13); lua_setglobal(L, "KEY_RETURN");
    lua_pushinteger(L, 32); lua_setglobal(L, "KEY_SPACE");
    
    // Screen modes
    lua_pushinteger(L, 0); lua_setglobal(L, "SCREEN_MODE_WINDOW");
    lua_pushinteger(L, 1); lua_setglobal(L, "SCREEN_MODE_FULLSCREEN");
}

void lua_mark_runtime_initialized() {
    g_runtime_pre_initialized = true;
}

int luaopen_abstract_runtime(lua_State* L) {
    // Register all function groups
    register_runtime_init_functions(L);
    register_display_functions(L);
    register_text_functions(L);
    register_cursor_functions(L);
    register_input_functions(L);
    register_sprite_functions(L);
    register_graphics_functions(L);
    register_cursor_functions(L);
    register_threading_functions(L);
    register_tile_functions(L);
    register_console_functions(L);
    register_utility_functions(L);
    register_runtime_constants(L);
    
    // Register LuaJIT compatibility functions
    luajit_register_compat_functions(L);
    
    return 0;
}