#ifndef LUA_BINDINGS_H
#define LUA_BINDINGS_H

#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <functional>

extern "C" {
// LuaJIT headers (compatible with Lua 5.1 API)
#include <luajit.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

/**
 * LuaJIT bindings for NewBCPL Abstract Runtime
 * This module provides LuaJIT access to all abstract runtime functions
 * for interactive testing and scripting with multi-threading support.
 */

// Forward declarations
struct LuaThread;
class LuaThreadManager;

/**
 * Lua Thread Handle - represents a running Lua script thread
 */
using LuaThreadHandle = std::shared_ptr<LuaThread>;

/**
 * Lua Thread Status
 */
enum class LuaThreadStatus {
    CREATED,    // Thread created but not started
    RUNNING,    // Thread is executing
    FINISHED,   // Thread completed successfully
    ERROR,      // Thread encountered an error
    STOPPED     // Thread was stopped by user
};

/**
 * Lua Thread Structure
 */
struct LuaThread {
    std::thread thread;
    lua_State* L;
    std::string filepath;
    std::string script_content;
    LuaThreadStatus status;
    std::string error_message;
    std::atomic<bool> should_stop{false};
    int thread_id;
    
    LuaThread(int id) : L(nullptr), status(LuaThreadStatus::CREATED), thread_id(id) {}
    ~LuaThread();
};

/**
 * Lua Thread Manager - manages multiple concurrent Lua threads
 */
class LuaThreadManager {
private:
    static std::unique_ptr<LuaThreadManager> instance;
    static std::mutex instance_mutex;
    
    std::vector<LuaThreadHandle> threads;
    mutable std::mutex threads_mutex;
    std::atomic<int> next_thread_id{1};
    std::atomic<bool> shutdown_requested{false};
    
    // Runtime API synchronization
    static std::mutex runtime_api_mutex;
    
    LuaThreadManager() = default;
    
public:
    static LuaThreadManager& getInstance();
    
    // Thread management
    LuaThreadHandle exec_lua(const std::string& filepath);
    LuaThreadHandle exec_lua_string(const std::string& script, const std::string& name = "inline");
    bool stop_lua_thread(LuaThreadHandle thread);
    void stop_all_threads();
    void cleanup_finished_threads();
    
    // Thread status queries
    std::vector<LuaThreadHandle> get_active_threads();
    std::vector<LuaThreadHandle> get_all_threads();
    int get_thread_count() const;
    
    // Shutdown
    void shutdown();
    
    // Runtime API synchronization helpers
    static void lock_runtime_api();
    static void unlock_runtime_api();
    
private:
    void thread_worker(LuaThreadHandle thread_handle);
    lua_State* create_thread_lua_state();
    void cleanup_lua_state(lua_State* L);
};

/**
 * RAII wrapper for runtime API synchronization
 */
class RuntimeAPILock {
public:
    RuntimeAPILock() { LuaThreadManager::lock_runtime_api(); }
    ~RuntimeAPILock() { LuaThreadManager::unlock_runtime_api(); }
};

// Macro for thread-safe runtime API calls
#define RUNTIME_API_CALL(call) \
    do { \
        RuntimeAPILock lock; \
        (call); \
    } while(0)

/**
 * Initialize all LuaJIT bindings for abstract runtime
 * This registers all runtime functions as Lua globals
 * @param L Lua state
 * @return 0 on success, non-zero on error
 */
int luaopen_abstract_runtime(lua_State* L);

/**
 * Initialize the LuaJIT thread manager
 * Must be called during runtime initialization
 */
void init_lua_thread_manager();

/**
 * Shutdown the LuaJIT thread manager
 * Stops all running threads and cleans up resources
 */
void shutdown_lua_thread_manager();

/**
 * Execute a Lua script file in a new thread
 * @param filepath Path to the Lua script file
 * @return Thread handle for the new Lua thread, or nullptr on error
 */
LuaThreadHandle exec_lua(const std::string& filepath);

/**
 * Execute a Lua script string in a new thread
 * @param script Lua script content
 * @param name Optional name for the script (for debugging)
 * @return Thread handle for the new Lua thread, or nullptr on error
 */
LuaThreadHandle exec_lua_string(const std::string& script, const std::string& name = "inline");

/**
 * Stop a running Lua thread
 * @param thread Thread handle to stop
 * @return true if thread was stopped, false if already finished or error
 */
bool stop_lua_thread(LuaThreadHandle thread);

/**
 * Stop all running Lua threads
 */
void stop_all_lua_threads();

/**
 * Get list of currently active Lua threads
 * @return Vector of active thread handles
 */
std::vector<LuaThreadHandle> get_active_lua_threads();

/**
 * Get the number of currently running Lua threads
 * @return Number of active threads
 */
int get_lua_thread_count();

/**
 * Register runtime initialization functions
 */
void register_runtime_init_functions(lua_State* L);

/**
 * Register display control functions
 */
void register_display_functions(lua_State* L);

/**
 * Register text system functions
 */
void register_text_functions(lua_State* L);

/**
 * Register cursor system functions
 */
void register_cursor_functions(lua_State* L);

/**
 * Register input system functions
 */
void register_input_functions(lua_State* L);

/**
 * Register graphics functions
 */
void register_graphics_functions(lua_State* L);

/**
 * Register sprite system functions
 */
void register_sprite_functions(lua_State* L);

/**
 * Register tile system functions
 */
void register_tile_functions(lua_State* L);

/**
 * Register utility helper functions
 */
void register_utility_functions(lua_State* L);

/**
 * Register threading functions for Lua scripts
 */
void register_threading_functions(lua_State* L);

/**
 * Set up runtime constants and enums
 */
void register_runtime_constants(lua_State* L);

// Sprite allocator function declarations
int lua_allocate_sprite(lua_State* L);
int lua_allocate_sprites(lua_State* L);
int lua_allocate_specific_sprite(lua_State* L);
int lua_release_sprite(lua_State* L);
int lua_release_all_sprites(lua_State* L);
int lua_get_owned_sprites(lua_State* L);
int lua_is_sprite_available(lua_State* L);
int lua_get_sprite_allocator_stats(lua_State* L);

// Console output function declarations
int lua_console_print(lua_State* L);
int lua_console_info(lua_State* L);
int lua_console_warning(lua_State* L);
int lua_console_error(lua_State* L);
int lua_console_debug(lua_State* L);
int lua_console_section(lua_State* L);
int lua_console_separator(lua_State* L);

/**
 * Error handling helper for Lua bindings
 */
int lua_runtime_error(lua_State* L, const char* message);

/**
 * Helper to check if runtime is initialized
 */
int check_runtime_initialized(lua_State* L);

/**
 * Helper to validate coordinate parameters
 */
int validate_coordinates(lua_State* L, int x, int y, const char* func_name);

/**
 * Helper to validate color parameters
 */
int validate_color(lua_State* L, int r, int g, int b, int a, const char* func_name);

/**
 * Mark runtime as pre-initialized (called when runtime is already running)
 */
void lua_mark_runtime_initialized();

/**
 * LuaJIT compatibility helpers for Lua 5.1 API differences
 */
const char* luajit_tolstring(lua_State* L, int idx, size_t* len);
void luajit_register_compat_functions(lua_State* L);

#endif // LUA_BINDINGS_H