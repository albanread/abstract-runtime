#include "abstract_runtime.h"
#include "sprite_bank.h"
#include "sprite_renderer.h"
#include "tile_layer.h"
#include "input_system.h"
#include "lua_bindings.h"


#include <SDL2/SDL.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cairo/cairo.h>
// MAX_INPUT_KEYS is defined in runtime_state.h

// Forward declarations for input system functions
// Forward declarations for input system functions
void update_key_state(int keycode, bool pressed);
void update_input_system();
void update_mouse_position(int x, int y);
void update_mouse_button(int button, bool pressed);
void update_mouse_wheel(int wheel_x, int wheel_y);
// Enhanced functions that also queue events for Phase 2
void update_key_state_with_event(int keycode, bool pressed, uint16_t sdl_modifiers);
void update_mouse_button_with_event(int button, bool pressed, int x, int y, uint16_t sdl_modifiers);
void update_mouse_position_with_event(int x, int y, uint16_t sdl_modifiers);
void update_mouse_wheel_with_event(int wheel_x, int wheel_y, uint16_t sdl_modifiers);
    // Runtime text input functions
    void update_runtime_text_input();
    void process_runtime_text_input_events();
    // REPL input functions
    void process_repl_input_events();
    void update_repl_input();
    // REPL overlay functions
    void process_repl_overlay_hotkeys();
    bool init_repl_overlay();
    void shutdown_repl_overlay();
    void handle_f8_pressed();
    void handle_f11_pressed();
    // Runtime form input functions
    void update_runtime_form_input();
    void process_runtime_form_input_events();
    // Runtime screen editor functions
    void update_runtime_screen_editor();
    void process_runtime_screen_editor_events();
extern int sdl_to_abstract_keycode(SDL_Keycode sdl_key);
extern uint32_t sdl_to_abstract_modifiers(uint16_t sdl_mod);

// =============================================================================
// GLOBAL STATE (SIMPLE AND WORKING)
// =============================================================================

// SDL and OpenGL state
static SDL_Window* g_window = nullptr;
static SDL_GLContext g_context = nullptr;
static std::atomic<bool> g_running(false);
static std::atomic<bool> g_quit_requested(false);
static std::atomic<bool> g_honor_sdl_quit(true);
static bool g_initialized = false;

// Screen configuration
static int g_screen_width = 800;
static int g_screen_height = 600;

// Background color (atomic for thread safety)
static std::atomic<int> g_bg_r{0};
static std::atomic<int> g_bg_g{0};
static std::atomic<int> g_bg_b{0};

// Text system - 32-bit Unicode text buffer for proper PETSCII support
// Thread safety: g_text_mutex protects all text buffer and color array access
// - Background thread (app) calls print_at(), clear_text(), etc. with mutex
// - Main thread (renderer) calls upload_text_to_texture() with mutex
// - Color setting functions (set_text_ink/paper) are atomic writes, no mutex needed
static uint32_t g_text_buffer[25][80]; // 25 rows, 80 Unicode codepoints
static int g_text_columns = 80;
static int g_text_rows = 25;

// =============================================================================
// CURSOR SYSTEM
// =============================================================================

enum CursorType {
    CURSOR_UNDERSCORE = 0,
    CURSOR_BLOCK = 1,
    CURSOR_VERTICAL_BAR = 2
};

struct TextCursor {
    int x, y;                    // Position in text grid
    bool visible;                // On/off state
    CursorType type;             // Cursor style
    uint32_t color;              // Cursor color (with alpha)
    bool blink_enabled;          // Whether cursor blinks
    uint64_t last_blink_time;    // For blink timing (milliseconds)
    bool blink_state;            // Current blink visibility state
    
    TextCursor() : x(0), y(0), visible(false), type(CURSOR_UNDERSCORE), 
                   color(0xFFFFFF80), blink_enabled(true), last_blink_time(0), 
                   blink_state(true) {}
};

static TextCursor g_text_cursor;
static std::mutex g_text_mutex; // Protects text buffer and color arrays
// Graphics system thread safety
static std::mutex g_graphics_mutex; // Protects graphics operations

// Text color maps - foreground (ink) and background (paper) colors
static uint32_t g_text_ink_colors[25][80];   // RGBA colors for text foreground
static uint32_t g_text_paper_colors[25][80]; // RGBA colors for text background

// Text backup system - expanding array of saved screens
struct TextBackup {
    uint32_t text_buffer[25][80];
    uint32_t ink_colors[25][80];
    uint32_t paper_colors[25][80];
};

static std::vector<TextBackup> g_text_backups;
static std::mutex g_backup_mutex;

// Frame synchronization for wait_for_render_complete
static std::atomic<uint64_t> g_frame_counter{0};
static std::mutex g_frame_sync_mutex;
static std::condition_variable g_frame_sync_cv;

// Current text colors for new text
static uint32_t g_current_ink_color = 0xFFFFFFFF;   // Default: opaque white
static uint32_t g_current_paper_color = 0x00000000; // Default: fully transparent

// Font rendering with FreeType
// Text system using FreeType
static FT_Library g_ft_library = nullptr;
static FT_Face g_ft_face = nullptr;
static unsigned char* g_text_bitmap = nullptr;
static GLuint g_text_texture = 0;
static bool g_text_dirty = true;  // Mark text as needing upload

// Cairo graphics layer (renders under text)
static cairo_surface_t* g_graphics_surface = nullptr;
static cairo_t* g_graphics_cr = nullptr;
static unsigned char* g_graphics_bitmap = nullptr;
static GLuint g_graphics_texture = 0;
static bool g_graphics_dirty = true;  // Mark graphics as needing upload

// Sprite system (renders between background and graphics)
AbstractRuntime::SpriteBank* g_sprite_bank = nullptr;
static AbstractRuntime::SpriteRenderer* g_sprite_renderer = nullptr;
static bool g_sprites_initialized = false;

// Front tile system (renders in front, original API)
static cairo_surface_t* g_tile_surface = nullptr;
static cairo_t* g_tile_cr = nullptr;
static unsigned char* g_tile_bitmap = nullptr;
static GLuint g_tile_texture = 0;
static bool g_tile_dirty = true;  // Mark tiles for upload
static bool g_tiles_initialized = false;

// Front tile world map (large, application-defined)
static int g_world_map_width = 0;
static int g_world_map_height = 0;
static int* g_world_map = nullptr;  // Large 2D world map stored as 1D

// Back tile system (renders behind front layer for parallax)
static cairo_surface_t* g_back_tile_surface = nullptr;
static cairo_t* g_back_tile_cr = nullptr;
static unsigned char* g_back_tile_bitmap = nullptr;
static GLuint g_back_tile_texture = 0;
static bool g_back_tile_dirty = true;  // Mark back tiles for upload

// Back tile world map (independent from front layer)
static int g_back_world_map_width = 0;
static int g_back_world_map_height = 0;
static int* g_back_world_map = nullptr;  // Back layer 2D world map

// Tile viewport (shared between both layers)
static int g_tile_grid_width = 0;   // Viewport width in tiles
static int g_tile_grid_height = 0;  // Viewport height in tiles
static float g_viewport_x = 0.0f;   // Front layer viewport position in world (pixels)
static float g_viewport_y = 0.0f;   // Front layer viewport position in world (pixels)

// Back layer viewport (independent scrolling for parallax)
static float g_back_viewport_x = 0.0f;   // Back layer viewport position in world (pixels)
static float g_back_viewport_y = 0.0f;   // Back layer viewport position in world (pixels)

static cairo_surface_t* g_tile_surfaces[256] = {nullptr};  // Loaded tile surfaces (shared)

// Tile view dimensions (screen + border, shared)
static int g_tile_view_width = 0;   // screen_width + 768
static int g_tile_view_height = 0;  // screen_height + 768

// Tile scrolling offset (in pixels)
static float g_tile_scroll_x = 0.0f;      // Front layer scroll
static float g_tile_scroll_y = 0.0f;      // Front layer scroll
static float g_back_tile_scroll_x = 0.0f; // Back layer scroll
static float g_back_tile_scroll_y = 0.0f; // Back layer scroll

// FPS tracking
static std::chrono::high_resolution_clock::time_point g_last_frame_time;
static float g_current_fps = 60.0f;
static float g_frame_time_ms = 16.67f;
static int g_frame_count = 0;

// Function pointer for user applications
using UserApplicationFunction = void(*)();

// =============================================================================
// FORWARD DECLARATIONS
// =============================================================================

static bool init_sdl_and_opengl(int screen_mode);
static bool init_font_system();
static bool init_graphics_system();
static bool init_sprite_system();
static void main_thread_loop();
static void render_frame();
static void upload_text_to_texture();
static void upload_graphics_to_texture();
static void render_text_texture_to_screen();
static void render_graphics_texture_to_screen();
static void upload_tiles_to_texture();
static void upload_back_tiles_to_texture();
static void render_tiles_texture_to_screen();
static void render_back_tiles_texture_to_screen();
static void render_sprites();
static void render_fps_overlay();
static void update_fps_stats();
static void cleanup_all();
static void clear_text_buffer();
static void init_text_colors();
static uint32_t pack_rgba(int r, int g, int b, int a);
static void unpack_rgba(uint32_t color, int* r, int* g, int* b, int* a);

// =============================================================================
// INITIALIZATION
// =============================================================================

bool init_abstract_runtime(int screen_mode) {
    if (g_initialized) {
        return true;
    }

    std::cout << "Initializing NewBCPL Abstract Runtime..." << std::endl;

    // Set screen dimensions based on text columns (16px font + margins)
    const int FONT_WIDTH = 16;
    const int FONT_HEIGHT = 24;  // Better line spacing for readability
    const int MARGIN_X = 40;     // Side margins
    const int MARGIN_Y = 50;     // Top/bottom margins (matches text rendering)
    const int TEXT_ROWS = 25;    // Standard terminal height
    
    switch (screen_mode) {
        case SCREEN_20_COLUMN:
            g_screen_width = (20 * FONT_WIDTH) + (2 * MARGIN_X);   // 320 + 80 = 400
            g_screen_height = (TEXT_ROWS * FONT_HEIGHT) + (2 * MARGIN_Y); // 550 + 100 = 650
            g_text_columns = 20;
            break;
        case SCREEN_40_COLUMN:
            g_screen_width = (40 * FONT_WIDTH) + (2 * MARGIN_X);   // 560 + 80 = 640
            g_screen_height = (TEXT_ROWS * FONT_HEIGHT) + (2 * MARGIN_Y); // 550 + 100 = 650
            g_text_columns = 40;
            break;
        case SCREEN_80_COLUMN:
            g_screen_width = (80 * FONT_WIDTH) + (2 * MARGIN_X) + 32;   // 1280 + 80 + 32 = 1392
            g_screen_height = (TEXT_ROWS * FONT_HEIGHT) + (2 * MARGIN_Y); // 550 + 100 = 650
            g_text_columns = 80;
            break;
        default:
            g_screen_width = (40 * FONT_WIDTH) + (2 * MARGIN_X);   // Default to 40 column
            g_screen_height = (TEXT_ROWS * FONT_HEIGHT) + (2 * MARGIN_Y); // 550 + 100 = 650
            g_text_columns = 40;
            break;
    }

    // Initialize SDL and OpenGL
    if (!init_sdl_and_opengl(screen_mode)) {
        std::cerr << "Failed to initialize SDL/OpenGL" << std::endl;
        return false;
    }

    // Initialize REPL overlay system
    if (!init_repl_overlay()) {
        std::cerr << "Failed to initialize REPL overlay system" << std::endl;
        return false;
    }

    // Initialize font system
    if (!init_font_system()) {
        std::cerr << "Failed to initialize font system" << std::endl;
        return false;
    }

    // Initialize graphics system
    if (!init_graphics_system()) {
        std::cerr << "Failed to initialize graphics system" << std::endl;
        return false;
    }

    // Clear text buffer and initialize colors
    clear_text_buffer();
    init_text_colors();

    // Initialize FPS timing
    g_last_frame_time = std::chrono::high_resolution_clock::now();
    g_frame_count = 0;

    // Initialize LuaJIT thread manager
    std::cout << "Initializing LuaJIT thread manager..." << std::endl;
    init_lua_thread_manager();

    g_initialized = true;
    std::cout << "✅ Abstract Runtime initialized successfully with LuaJIT multi-threading!" << std::endl;
    return true;
}

void shutdown_abstract_runtime() {
    if (!g_initialized) return;

    std::cout << "Shutting down Abstract Runtime..." << std::endl;
    g_quit_requested.store(true);
    
    // Shutdown REPL overlay system
    shutdown_repl_overlay();
    
    // Shutdown LuaJIT thread manager
    shutdown_lua_thread_manager();
    
    cleanup_all();
    g_initialized = false;
    std::cout << "✅ Runtime shutdown complete!" << std::endl;
}

// =============================================================================
// RUNTIME CONTROL
// =============================================================================

int run_main_loop() {
    if (!g_initialized) {
        std::cerr << "Runtime not initialized" << std::endl;
        return 1;
    }

    g_running.store(true);
    main_thread_loop();
    return 0;
}

int run_runtime_with_app(int screen_mode, UserApplicationFunction user_app_func) {
    std::cout << "NewBCPL Abstract Runtime - Starting with background user app" << std::endl;

    // Initialize runtime
    if (!init_abstract_runtime(screen_mode)) {
        std::cerr << "Failed to initialize runtime" << std::endl;
        return 1;
    }

    // Start user application on background thread
    std::thread user_thread([user_app_func]() {
        try {
            std::cout << "[User Thread] Starting user application..." << std::endl;
            user_app_func();
            std::cout << "[User Thread] User application completed" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[User Thread] Exception: " << e.what() << std::endl;
        }

        // User app finished, but keep runtime running until explicitly requested to quit
        // g_quit_requested.store(true); // Commented out to prevent automatic exit
    });

    // Run main rendering loop on main thread (required for macOS)
    int result = run_main_loop();

    // Detach user thread - let it die when process exits
    std::cout << "Main rendering loop finished" << std::endl;
    if (user_thread.joinable()) {
        user_thread.detach();
    }

    // Cleanup
    shutdown_abstract_runtime();
    return result;
}

// =============================================================================
// RUNTIME CONTROL
// =============================================================================

void exit_runtime() {
    g_quit_requested.store(true);
}

void disable_auto_quit() {
    g_honor_sdl_quit.store(false);
}

void enable_auto_quit() {
    g_honor_sdl_quit.store(true);
}

bool should_quit() {
    return g_quit_requested.load();
}

void* get_ft_face() {
    return g_ft_face;
}

// =============================================================================
// SDL AND OPENGL INITIALIZATION
// =============================================================================

static bool init_sdl_and_opengl(int screen_mode) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Set OpenGL 2.1 compatibility for reliable rendering
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    g_window = SDL_CreateWindow(
        "NewBCPL Abstract Runtime",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        g_screen_width, g_screen_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }

    g_context = SDL_GL_CreateContext(g_window);
    if (!g_context) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }

    // Enable VSync
    SDL_GL_SetSwapInterval(1);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

// =============================================================================
// FONT SYSTEM INITIALIZATION
// =============================================================================

static bool init_font_system() {
    // Initialize FreeType
    if (FT_Init_FreeType(&g_ft_library)) {
        std::cerr << "Failed to initialize FreeType" << std::endl;
        return false;
    }

    // Load font - try PetMe font first, fallback to system font
    const char* font_paths[] = {
        "assets/fonts/petme/PetMe.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Courier New.ttf"
    };

    bool font_loaded = false;
    for (const char* font_path : font_paths) {
        if (FT_New_Face(g_ft_library, font_path, 0, &g_ft_face) == 0) {
            std::cout << "Loaded font: " << font_path << std::endl;
            font_loaded = true;
            break;
        }
    }

    if (!font_loaded) {
        std::cerr << "Failed to load any font" << std::endl;
        return false;
    }

    // Set font size
    FT_Set_Pixel_Sizes(g_ft_face, 0, 16);

    // Create text bitmap buffer (RGBA)
    g_text_bitmap = new unsigned char[g_screen_width * g_screen_height * 4];
    memset(g_text_bitmap, 0, g_screen_width * g_screen_height * 4);

    // Create OpenGL texture for text
    glGenTextures(1, &g_text_texture);

    std::cout << "Font system initialized" << std::endl;
    return true;
}

// =============================================================================
// GRAPHICS SYSTEM INITIALIZATION  
// =============================================================================

static bool init_graphics_system() {
    // Create Cairo surface for graphics (ARGB32 format)
    g_graphics_bitmap = new unsigned char[g_screen_width * g_screen_height * 4];
    memset(g_graphics_bitmap, 0, g_screen_width * g_screen_height * 4);

    g_graphics_surface = cairo_image_surface_create_for_data(
        g_graphics_bitmap,
        CAIRO_FORMAT_ARGB32,
        g_screen_width,
        g_screen_height,
        g_screen_width * 4
    );

    if (cairo_surface_status(g_graphics_surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "Failed to create Cairo surface" << std::endl;
        return false;
    }

    g_graphics_cr = cairo_create(g_graphics_surface);
    if (cairo_status(g_graphics_cr) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "Failed to create Cairo context" << std::endl;
        return false;
    }

    // Create OpenGL texture for graphics
    glGenTextures(1, &g_graphics_texture);
    
    // Create OpenGL texture for tiles
    glGenTextures(1, &g_tile_texture);
    
    // Create OpenGL texture for back tiles
    glGenTextures(1, &g_back_tile_texture);

    // Set default line width
    cairo_set_line_width(g_graphics_cr, 1.0);

    std::cout << "Graphics system initialized" << std::endl;
    return true;
}

static bool init_sprite_system() {
    std::cout << "[Runtime] Initializing sprite system..." << std::endl;
    
    // Create sprite bank
    g_sprite_bank = new AbstractRuntime::SpriteBank();
    if (!g_sprite_bank->initialize()) {
        std::cerr << "Failed to initialize sprite bank" << std::endl;
        delete g_sprite_bank;
        g_sprite_bank = nullptr;
        return false;
    }

    // Create sprite renderer
    g_sprite_renderer = new AbstractRuntime::SpriteRenderer();
    if (!g_sprite_renderer->initialize(g_sprite_bank, g_screen_width, g_screen_height)) {
        std::cerr << "Failed to initialize sprite renderer" << std::endl;
        delete g_sprite_renderer;
        g_sprite_renderer = nullptr;
        delete g_sprite_bank;
        g_sprite_bank = nullptr;
        return false;
    }

    g_sprites_initialized = true;
    std::cout << "[Runtime] Sprite system initialized successfully" << std::endl;
    return true;
}

// =============================================================================
// RENDERING PIPELINE
// =============================================================================

static void main_thread_loop() {
    std::cout << "Starting main rendering loop..." << std::endl;

    while (!g_quit_requested.load()) {
        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                if (g_honor_sdl_quit.load()) {
                    g_quit_requested.store(true);
                    break;
                }
            }
            // Process keyboard events for input system
            else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                // Check for F8/F9 hotkeys first for immediate response
                if (event.type == SDL_KEYDOWN) {
                    // Debug all function keys F1-F12
                    if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F12) {
                        int fn_num = event.key.keysym.sym - SDLK_F1 + 1;
                        std::cout << "[SDL DEBUG] Function key F" << fn_num << " pressed (SDL code: " << event.key.keysym.sym << ")" << std::endl;
                    }
                    
                    if (event.key.keysym.sym == SDLK_F8) {
                        std::cout << "[SDL DEBUG] F8 detected, calling handler" << std::endl;
                        handle_f8_pressed();
                    }
                    else if (event.key.keysym.sym == SDLK_F11) {
                        std::cout << "[SDL DEBUG] F11 detected, calling handler" << std::endl;
                        handle_f11_pressed();
                    }
                }
                
                // Debug: Print all SDL keycodes to see what arrow keys send
                // Debug prints disabled for cleaner REPL experience
                
                // Get runtime state for input system
                extern void* g_runtime_state;
                if (g_runtime_state) {
                    // Convert SDL keycode to abstract keycode
                    int abstract_keycode = sdl_to_abstract_keycode(event.key.keysym.sym);
                    
                    // Debug: Print conversion result
                    // Debug: keycode conversion
                    
                    if (abstract_keycode > 0 && abstract_keycode < 512) {  // Match actual key_states array size
                        // Update both immediate state and event queues (Phase 2)
                        update_key_state_with_event(abstract_keycode, event.type == SDL_KEYDOWN, event.key.keysym.mod);
                        
                        // Debug: Confirm key state was updated
                        // Debug: key state updated
                    } else {
                        // Debug: Print why keycode was rejected
                        // Debug: keycode out of bounds
                    }
                }
            }
            // Process mouse events for input system
            else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                // Debug prints disabled for cleaner REPL experience
                
                // Update mouse position and button state with events (Phase 2)
                int button_index = event.button.button - 1;
                if (button_index >= 0 && button_index < 8) {  // Support up to 8 mouse buttons
                    update_mouse_button_with_event(button_index, event.type == SDL_MOUSEBUTTONDOWN, 
                                                   event.button.x, event.button.y, 0); // TODO: Get modifier state
                    // Debug: mouse button updated
                }
            }
            // Process mouse motion
            else if (event.type == SDL_MOUSEMOTION) {
                update_mouse_position_with_event(event.motion.x, event.motion.y, 0); // TODO: Get modifier state
                // Note: Not printing debug for mouse motion as it would spam the console
            }
            // Process mouse wheel
            else if (event.type == SDL_MOUSEWHEEL) {
                // Debug: mouse wheel event
                update_mouse_wheel_with_event(event.wheel.x, event.wheel.y, 0); // TODO: Get modifier state
            }
        }

        // Process REPL overlay hotkeys (F8/F9) - now handled at SDL event level for immediate response
        // process_repl_overlay_hotkeys();
        
        // Process REPL input events first
        process_repl_input_events();
        
        // Process runtime text input events
        process_runtime_text_input_events();
        
        // Process runtime form input events
        process_runtime_form_input_events();
        
        // Process screen editor events
        process_runtime_screen_editor_events();
        
        // Process character input events (RDCH system)
        extern void process_char_input_events();
        process_char_input_events();

        // Update input system frame management  
        update_input_system();
        
        // Update runtime text input sessions
        update_runtime_text_input();
        
        // Update REPL input sessions
        update_repl_input();
        
        // Update runtime form input sessions
        update_runtime_form_input();
        
        // Update screen editor sessions
        update_runtime_screen_editor();

        // Process sprite load queue (CRITICAL - sprites won't load without this!)
        if (g_sprite_bank) {
            g_sprite_bank->process_load_queue();
        }

        // Render frame
        render_frame();

        // Update FPS stats
        update_fps_stats();

        // VSync handles timing - no manual delay needed
    }

    std::cout << "Main rendering loop finished" << std::endl;
}

static void render_frame() {
    // Clear with background color
    float r = g_bg_r.load() / 255.0f;
    float g = g_bg_g.load() / 255.0f;
    float b = g_bg_b.load() / 255.0f;

    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Upload textures to GPU only when dirty
    if (g_graphics_dirty) {
        upload_graphics_to_texture();
        g_graphics_dirty = false;
    }
    
    if (g_tile_dirty) {
        upload_tiles_to_texture();
        g_tile_dirty = false;
    }
    
    if (g_back_tile_dirty) {
        upload_back_tiles_to_texture();
        g_back_tile_dirty = false;
    }
    if (g_text_dirty) {
        upload_text_to_texture();
        g_text_dirty = false;
    }
    
    // Render layers in correct Z-order (back to front):
    // 1. Back tiles (far background for parallax)
    render_back_tiles_texture_to_screen();
    // 2. Front tiles (main background terrain)  
    render_tiles_texture_to_screen();
    // 3. Graphics (Cairo vector graphics)
    render_graphics_texture_to_screen();
    // 4. Sprites (should be under text)
    render_sprites();
    // 5. Text (should be on top of sprites) 
    render_text_texture_to_screen();
    // 6. FPS overlay (on top of everything)
    render_fps_overlay();

    // Present frame
    SDL_GL_SwapWindow(g_window);
    g_frame_count++;
    
    // Notify waiting threads that frame is complete
    {
        std::lock_guard<std::mutex> lock(g_frame_sync_mutex);
        g_frame_counter++;
    }
    g_frame_sync_cv.notify_all();
}

static void upload_tiles_to_texture() {
    if (!g_tiles_initialized || !g_tile_cr) return;
    
    // Clear the tile surface
    cairo_save(g_tile_cr);
    cairo_set_operator(g_tile_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(g_tile_cr);
    cairo_restore(g_tile_cr);
    
    // Calculate which portion of world map is visible
    int world_start_x = (int)(g_viewport_x / 128.0f);
    int world_start_y = (int)(g_viewport_y / 128.0f);
    
    // Draw tiles from world map to viewport texture
    for (int grid_y = 0; grid_y < g_tile_grid_height; grid_y++) {
        for (int grid_x = 0; grid_x < g_tile_grid_width; grid_x++) {
            // Calculate world map coordinates
            int world_x = world_start_x + grid_x;
            int world_y = world_start_y + grid_y;
            
            int tile_id = 0;
            if (world_x >= 0 && world_x < g_world_map_width && 
                world_y >= 0 && world_y < g_world_map_height) {
                tile_id = g_world_map[world_y * g_world_map_width + world_x];
            }
            
            if (tile_id > 0 && tile_id < 256 && g_tile_surfaces[tile_id]) {
                // Calculate position in tile view (128 pixel grid)
                double dest_x = grid_x * 128.0;
                double dest_y = grid_y * 128.0;
                
                // Draw tile to Cairo surface
                cairo_set_source_surface(g_tile_cr, g_tile_surfaces[tile_id], dest_x, dest_y);
                cairo_rectangle(g_tile_cr, dest_x, dest_y, 128, 128);
                cairo_fill(g_tile_cr);
            }
        }
    }
    
    // Upload Cairo bitmap to OpenGL texture (same as graphics system)
    glBindTexture(GL_TEXTURE_2D, g_tile_texture);
    
    // Convert CAIRO_FORMAT_ARGB32 to GL_RGBA
    unsigned char* rgba_data = new unsigned char[g_tile_view_width * g_tile_view_height * 4];
    for (int i = 0; i < g_tile_view_width * g_tile_view_height; i++) {
        // Cairo ARGB32 is stored as BGRA in memory on little-endian
        rgba_data[i * 4 + 0] = g_tile_bitmap[i * 4 + 2]; // R
        rgba_data[i * 4 + 1] = g_tile_bitmap[i * 4 + 1]; // G
        rgba_data[i * 4 + 2] = g_tile_bitmap[i * 4 + 0]; // B
        rgba_data[i * 4 + 3] = g_tile_bitmap[i * 4 + 3]; // A
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_tile_view_width, g_tile_view_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    delete[] rgba_data;
}

static void upload_back_tiles_to_texture() {
    if (!g_tiles_initialized || !g_back_tile_cr) return;
    
    // Clear the back tile surface
    cairo_save(g_back_tile_cr);
    cairo_set_operator(g_back_tile_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(g_back_tile_cr);
    cairo_restore(g_back_tile_cr);
    
    // Calculate which portion of back world map is visible
    int world_start_x = (int)(g_back_viewport_x / 128.0f);
    int world_start_y = (int)(g_back_viewport_y / 128.0f);
    
    // Draw tiles from back world map to viewport texture
    for (int grid_y = 0; grid_y < g_tile_grid_height; grid_y++) {
        for (int grid_x = 0; grid_x < g_tile_grid_width; grid_x++) {
            // Calculate world map coordinates
            int world_x = world_start_x + grid_x;
            int world_y = world_start_y + grid_y;
            
            int tile_id = 0;
            if (world_x >= 0 && world_x < g_back_world_map_width && 
                world_y >= 0 && world_y < g_back_world_map_height) {
                tile_id = g_back_world_map[world_y * g_back_world_map_width + world_x];
            }
            
            if (tile_id > 0 && tile_id < 256 && g_tile_surfaces[tile_id]) {
                // Calculate position in tile view (128 pixel grid)
                double dest_x = grid_x * 128.0;
                double dest_y = grid_y * 128.0;
                
                // Draw tile to Cairo surface
                cairo_set_source_surface(g_back_tile_cr, g_tile_surfaces[tile_id], dest_x, dest_y);
                cairo_rectangle(g_back_tile_cr, dest_x, dest_y, 128, 128);
                cairo_fill(g_back_tile_cr);
            }
        }
    }
    
    // Upload Cairo bitmap to OpenGL texture (same as graphics system)
    glBindTexture(GL_TEXTURE_2D, g_back_tile_texture);
    
    // Convert CAIRO_FORMAT_ARGB32 to GL_RGBA
    unsigned char* rgba_data = new unsigned char[g_tile_view_width * g_tile_view_height * 4];
    for (int i = 0; i < g_tile_view_width * g_tile_view_height; i++) {
        // Cairo ARGB32 is stored as BGRA in memory on little-endian
        rgba_data[i * 4 + 0] = g_back_tile_bitmap[i * 4 + 2]; // R
        rgba_data[i * 4 + 1] = g_back_tile_bitmap[i * 4 + 1]; // G
        rgba_data[i * 4 + 2] = g_back_tile_bitmap[i * 4 + 0]; // B
        rgba_data[i * 4 + 3] = g_back_tile_bitmap[i * 4 + 3]; // A
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_tile_view_width, g_tile_view_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    delete[] rgba_data;
}

static void render_tiles_texture_to_screen() {
    if (!g_tiles_initialized) return;
    
    // Render fullscreen quad with tile texture (same as graphics system)
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindTexture(GL_TEXTURE_2D, g_tile_texture);
    
    // Set up orthographic projection for tile rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_screen_width, g_screen_height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Render tile texture with scrolling offset
    // Base texture coordinates for the center portion (384px border)
    float base_tex_left = 384.0f / g_tile_view_width;
    float base_tex_right = (g_tile_view_width - 384.0f) / g_tile_view_width;
    float base_tex_top = 384.0f / g_tile_view_height;
    float base_tex_bottom = (g_tile_view_height - 384.0f) / g_tile_view_height;
    
    // Apply scroll offset to texture coordinates
    float scroll_offset_x = g_tile_scroll_x / g_tile_view_width;
    float scroll_offset_y = g_tile_scroll_y / g_tile_view_height;
    
    float tex_left = base_tex_left + scroll_offset_x;
    float tex_right = base_tex_right + scroll_offset_x;
    float tex_top = base_tex_top + scroll_offset_y;
    float tex_bottom = base_tex_bottom + scroll_offset_y;
    
    // Render fullscreen quad with scrolled texture coordinates
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(tex_left, tex_top);    glVertex2f(0, 0);
    glTexCoord2f(tex_right, tex_top);   glVertex2f(g_screen_width, 0);
    glTexCoord2f(tex_right, tex_bottom); glVertex2f(g_screen_width, g_screen_height);
    glTexCoord2f(tex_left, tex_bottom);  glVertex2f(0, g_screen_height);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void render_back_tiles_texture_to_screen() {
    if (!g_tiles_initialized) return;
    
    // Render fullscreen quad with back tile texture (renders first/behind)
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindTexture(GL_TEXTURE_2D, g_back_tile_texture);
    
    // Set up orthographic projection for tile rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_screen_width, g_screen_height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Render back tile texture with independent scrolling offset
    // Base texture coordinates for the center portion (384px border)
    float base_tex_left = 384.0f / g_tile_view_width;
    float base_tex_right = (g_tile_view_width - 384.0f) / g_tile_view_width;
    float base_tex_top = 384.0f / g_tile_view_height;
    float base_tex_bottom = (g_tile_view_height - 384.0f) / g_tile_view_height;
    
    // Apply back layer scroll offset to texture coordinates
    float scroll_offset_x = g_back_tile_scroll_x / g_tile_view_width;
    float scroll_offset_y = g_back_tile_scroll_y / g_tile_view_height;
    
    float tex_left = base_tex_left + scroll_offset_x;
    float tex_right = base_tex_right + scroll_offset_x;
    float tex_top = base_tex_top + scroll_offset_y;
    float tex_bottom = base_tex_bottom + scroll_offset_y;
    
    // Render fullscreen quad with scrolled texture coordinates
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(tex_left, tex_top);    glVertex2f(0, 0);
    glTexCoord2f(tex_right, tex_top);   glVertex2f(g_screen_width, 0);
    glTexCoord2f(tex_right, tex_bottom); glVertex2f(g_screen_width, g_screen_height);
    glTexCoord2f(tex_left, tex_bottom);  glVertex2f(0, g_screen_height);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void render_sprites() {
    if (g_sprites_initialized && g_sprite_renderer) {
        // Process any pending sprite load requests from background threads
        if (g_sprite_bank) {
            g_sprite_bank->process_load_queue();
        }
        
        // Render sprites
        g_sprite_renderer->render_sprites();
    }
}

static void upload_graphics_to_texture() {
    // Upload graphics bitmap to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, g_graphics_texture);

    // Convert CAIRO_FORMAT_ARGB32 to GL_RGBA
    unsigned char* rgba_data = new unsigned char[g_screen_width * g_screen_height * 4];
    for (int i = 0; i < g_screen_width * g_screen_height; i++) {
        // Cairo ARGB32 is stored as BGRA in memory on little-endian
        rgba_data[i * 4 + 0] = g_graphics_bitmap[i * 4 + 2]; // R
        rgba_data[i * 4 + 1] = g_graphics_bitmap[i * 4 + 1]; // G
        rgba_data[i * 4 + 2] = g_graphics_bitmap[i * 4 + 0]; // B
        rgba_data[i * 4 + 3] = g_graphics_bitmap[i * 4 + 3]; // A
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_screen_width, g_screen_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    delete[] rgba_data;
}

static void render_graphics_texture_to_screen() {
    // Render fullscreen quad with graphics texture UNDER text layer
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, g_graphics_texture);

    // Set up orthographic projection for graphics rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_screen_width, g_screen_height, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Draw graphics layer quad (rendered first = underneath text)
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(g_screen_width, 0);
        glTexCoord2f(1, 1); glVertex2f(g_screen_width, g_screen_height);
        glTexCoord2f(0, 1); glVertex2f(0, g_screen_height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void upload_text_to_texture() {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    
    // Clear text bitmap (transparent background)
    memset(g_text_bitmap, 0, g_screen_width * g_screen_height * 4);

    // Render each character cell from text buffer
    for (int row = 0; row < g_text_rows; row++) {
        for (int col = 0; col < g_text_columns; col++) {
            // Calculate character cell position with baseline
            int cell_x = col * 16 + 15; // 16 pixels per character + margin
            int cell_y = row * 24 + 50; // 24 pixels per line + margin for better spacing
            int baseline_y = cell_y + 14; // Baseline is 14 pixels from top of cell
            
            // Get colors for this cell
            int paper_r, paper_g, paper_b, paper_a;
            int ink_r, ink_g, ink_b, ink_a;
            unpack_rgba(g_text_paper_colors[row][col], &paper_r, &paper_g, &paper_b, &paper_a);
            unpack_rgba(g_text_ink_colors[row][col], &ink_r, &ink_g, &ink_b, &ink_a);
            
            // Draw background (paper) if not transparent
            if (paper_a > 0) {
                // Draw background for full character cell (24 pixels high to match row spacing)
                for (int py = 0; py < 24 && (cell_y + py) < g_screen_height; py++) {
                    for (int px = 0; px < 16 && (cell_x + px) < g_screen_width; px++) {
                        int tx = cell_x + px;
                        int ty = cell_y + py;
                        if (tx >= 0 && ty >= 0) {
                            int pixel_index = (ty * g_screen_width + tx) * 4;
                            g_text_bitmap[pixel_index + 0] = paper_r;
                            g_text_bitmap[pixel_index + 1] = paper_g;
                            g_text_bitmap[pixel_index + 2] = paper_b;
                            g_text_bitmap[pixel_index + 3] = paper_a;
                        }
                    }
                }
            }
            
            // Draw character (foreground) if not space
            uint32_t unicode_char = g_text_buffer[row][col];
            if (unicode_char != 0x20 && unicode_char != 0 && ink_a > 0) {
                // Load character glyph using Unicode codepoint
                if (FT_Load_Char(g_ft_face, unicode_char, FT_LOAD_RENDER) == 0) {
                    FT_GlyphSlot glyph = g_ft_face->glyph;

                    // Copy glyph bitmap to text bitmap using baseline positioning
                    for (int gy = 0; gy < (int)glyph->bitmap.rows; gy++) {
                        for (int gx = 0; gx < (int)glyph->bitmap.width; gx++) {
                            int tx = cell_x + gx + glyph->bitmap_left;
                            int ty = baseline_y + gy - glyph->bitmap_top;

                            if (tx >= 0 && tx < g_screen_width && ty >= 0 && ty < g_screen_height) {
                                // Handle FreeType pitch correctly
                                int glyph_index = gy * glyph->bitmap.pitch + gx;
                                unsigned char glyph_alpha = glyph->bitmap.buffer[glyph_index];
                                
                                if (glyph_alpha > 0) {
                                    int pixel_index = (ty * g_screen_width + tx) * 4;
                                    // Blend glyph alpha with ink alpha
                                    int final_alpha = (glyph_alpha * ink_a) / 255;
                                    g_text_bitmap[pixel_index + 0] = ink_r;
                                    g_text_bitmap[pixel_index + 1] = ink_g;
                                    g_text_bitmap[pixel_index + 2] = ink_b;
                                    g_text_bitmap[pixel_index + 3] = final_alpha;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Render cursor overlay if visible
    if (g_text_cursor.visible && g_text_cursor.x >= 0 && g_text_cursor.x < g_text_columns && 
        g_text_cursor.y >= 0 && g_text_cursor.y < g_text_rows) {
        
        // Handle cursor blinking
        bool should_draw_cursor = true;
        if (g_text_cursor.blink_enabled) {
            uint64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            // Blink every 500ms
            if (current_time - g_text_cursor.last_blink_time > 500) {
                g_text_cursor.blink_state = !g_text_cursor.blink_state;
                g_text_cursor.last_blink_time = current_time;
            }
            should_draw_cursor = g_text_cursor.blink_state;
        }
        
        if (should_draw_cursor) {
            // Calculate cursor position using same positioning as text rendering
            int cursor_cell_x = g_text_cursor.x * 16 + 15;
            int cursor_cell_y = g_text_cursor.y * 24 + 50;
            int baseline_y = cursor_cell_y + 14; // Same baseline as text rendering
            
            // Get cursor color components
            int cursor_r, cursor_g, cursor_b, cursor_a;
            unpack_rgba(g_text_cursor.color, &cursor_r, &cursor_g, &cursor_b, &cursor_a);
            
            // Draw cursor based on type
            switch (g_text_cursor.type) {
                case CURSOR_UNDERSCORE:
                    // Draw underscore at bottom of character cell
                    for (int cy = baseline_y + 2; cy < baseline_y + 5 && cy < g_screen_height; cy++) {
                        for (int cx = cursor_cell_x; cx < cursor_cell_x + 16 && cx < g_screen_width; cx++) {
                            if (cx >= 0 && cy >= 0) {
                                int pixel_index = (cy * g_screen_width + cx) * 4;
                                // Alpha blend cursor color with existing pixel
                                g_text_bitmap[pixel_index + 0] = cursor_r;
                                g_text_bitmap[pixel_index + 1] = cursor_g;
                                g_text_bitmap[pixel_index + 2] = cursor_b;
                                g_text_bitmap[pixel_index + 3] = cursor_a;
                            }
                        }
                    }
                    break;
                    
                case CURSOR_BLOCK:
                    // Draw full character block
                    for (int cy = cursor_cell_y; cy < cursor_cell_y + 24 && cy < g_screen_height; cy++) {
                        for (int cx = cursor_cell_x; cx < cursor_cell_x + 16 && cx < g_screen_width; cx++) {
                            if (cx >= 0 && cy >= 0) {
                                int pixel_index = (cy * g_screen_width + cx) * 4;
                                g_text_bitmap[pixel_index + 0] = cursor_r;
                                g_text_bitmap[pixel_index + 1] = cursor_g;
                                g_text_bitmap[pixel_index + 2] = cursor_b;
                                g_text_bitmap[pixel_index + 3] = cursor_a;
                            }
                        }
                    }
                    break;
                    
                case CURSOR_VERTICAL_BAR:
                    // Draw vertical bar at left edge of character cell
                    for (int cy = cursor_cell_y; cy < cursor_cell_y + 24 && cy < g_screen_height; cy++) {
                        for (int cx = cursor_cell_x; cx < cursor_cell_x + 2 && cx < g_screen_width; cx++) {
                            if (cx >= 0 && cy >= 0) {
                                int pixel_index = (cy * g_screen_width + cx) * 4;
                                g_text_bitmap[pixel_index + 0] = cursor_r;
                                g_text_bitmap[pixel_index + 1] = cursor_g;
                                g_text_bitmap[pixel_index + 2] = cursor_b;
                                g_text_bitmap[pixel_index + 3] = cursor_a;
                            }
                        }
                    }
                    break;
            }
        }
    }

    // Upload text bitmap to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, g_text_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_screen_width, g_screen_height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, g_text_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void render_text_texture_to_screen() {
    // Set up orthographic projection for text rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_screen_width, g_screen_height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Render text layer on top with blending
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glBindTexture(GL_TEXTURE_2D, g_text_texture);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(g_screen_width, 0);
        glTexCoord2f(1, 1); glVertex2f(g_screen_width, g_screen_height);
        glTexCoord2f(0, 1); glVertex2f(0, g_screen_height);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void render_fps_overlay() {
    // Create FPS text string
    char fps_text[32];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", g_current_fps);
    
    // Calculate position in top-right corner (14pt font needs more space)
    int char_width = 12;
    int char_height = 18;
    int text_len = strlen(fps_text);
    int padding = 10;
    
    int text_width = text_len * char_width;
    int start_x = g_screen_width - text_width - padding;
    int start_y = padding;
    
    // Create small bitmap just for FPS text
    int fps_width = text_width + 8;  // Small padding
    int fps_height = char_height + 4;
    unsigned char* fps_bitmap = new unsigned char[fps_width * fps_height * 4];
    memset(fps_bitmap, 0, fps_width * fps_height * 4);  // Transparent
    
    // Render each character to the small bitmap
    for (int i = 0; i < text_len; i++) {
        char ch = fps_text[i];
        if (ch != ' ' && ch != '\0') {
            if (FT_Load_Char(g_ft_face, ch, FT_LOAD_RENDER) == 0) {
                FT_GlyphSlot glyph = g_ft_face->glyph;
                
                int char_x = 4 + i * char_width;  // Small left padding
                int char_y = 2;  // Small top padding
                
                // Copy glyph to FPS bitmap (yellow text)
                for (int gy = 0; gy < (int)glyph->bitmap.rows; gy++) {
                    for (int gx = 0; gx < (int)glyph->bitmap.width; gx++) {
                        int tx = char_x + gx + glyph->bitmap_left;
                        int ty = char_y + gy + (char_height - glyph->bitmap_top);
                        
                        if (tx >= 0 && tx < fps_width && ty >= 0 && ty < fps_height) {
                            int glyph_index = gy * glyph->bitmap.pitch + gx;
                            unsigned char alpha = glyph->bitmap.buffer[glyph_index];
                            
                            if (alpha > 0) {
                                int pixel_index = (ty * fps_width + tx) * 4;
                                fps_bitmap[pixel_index + 0] = 255;   // R (yellow)
                                fps_bitmap[pixel_index + 1] = 255;   // G (yellow)  
                                fps_bitmap[pixel_index + 2] = 0;     // B (yellow)
                                fps_bitmap[pixel_index + 3] = alpha; // A (preserve alpha)
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Upload small bitmap to texture
    GLuint fps_texture;
    glGenTextures(1, &fps_texture);
    glBindTexture(GL_TEXTURE_2D, fps_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fps_width, fps_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, fps_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    delete[] fps_bitmap;
    
    // Set up 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, g_screen_width, g_screen_height, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Render ONLY the small FPS area with transparency
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Render just the small FPS quad - NOT the entire screen!
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(start_x - 4, start_y);
        glTexCoord2f(1, 0); glVertex2i(start_x - 4 + fps_width, start_y);
        glTexCoord2f(1, 1); glVertex2i(start_x - 4 + fps_width, start_y + fps_height);
        glTexCoord2f(0, 1); glVertex2i(start_x - 4, start_y + fps_height);
    glEnd();
    
    // Cleanup
    glDeleteTextures(1, &fps_texture);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

static void update_fps_stats() {
    auto current_time = std::chrono::high_resolution_clock::now();
    auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(current_time - g_last_frame_time);
    g_last_frame_time = current_time;

    // Convert to milliseconds
    float frame_ms = frame_duration.count() / 1000.0f;

    // Smooth the frame time with exponential moving average
    g_frame_time_ms = g_frame_time_ms * 0.9f + frame_ms * 0.1f;

    // Calculate FPS
    if (g_frame_time_ms > 0.0f) {
        g_current_fps = 1000.0f / g_frame_time_ms;
    }
}

// =============================================================================
// TEXT API IMPLEMENTATION
// =============================================================================

void print_at(int x, int y, const char* text) {
    if (!g_initialized || !text) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;

    std::lock_guard<std::mutex> lock(g_text_mutex);
    int len = strlen(text);
    for (int i = 0; i < len && (x + i) < g_text_columns; i++) {
        unsigned char ch = (unsigned char)text[i];
        g_text_buffer[y][x + i] = ch;  // Store as Unicode codepoint directly
        // Set colors for this character position
        g_text_ink_colors[y][x + i] = g_current_ink_color;
        g_text_paper_colors[y][x + i] = g_current_paper_color;
    }
    g_text_dirty = true;  // Mark text for upload
}

// UTF-8 decoding helper function
static uint32_t decode_utf8_char(const char*& utf8_str) {
    const unsigned char* str = (const unsigned char*)utf8_str;
    uint32_t codepoint = 0;
    
    if ((*str & 0x80) == 0) {
        // 1-byte character (ASCII)
        codepoint = *str;
        utf8_str += 1;
    } else if ((*str & 0xE0) == 0xC0) {
        // 2-byte character
        codepoint = (*str & 0x1F) << 6;
        codepoint |= (*(str + 1) & 0x3F);
        utf8_str += 2;
    } else if ((*str & 0xF0) == 0xE0) {
        // 3-byte character
        codepoint = (*str & 0x0F) << 12;
        codepoint |= (*(str + 1) & 0x3F) << 6;
        codepoint |= (*(str + 2) & 0x3F);
        utf8_str += 3;
    } else if ((*str & 0xF8) == 0xF0) {
        // 4-byte character
        codepoint = (*str & 0x07) << 18;
        codepoint |= (*(str + 1) & 0x3F) << 12;
        codepoint |= (*(str + 2) & 0x3F) << 6;
        codepoint |= (*(str + 3) & 0x3F);
        utf8_str += 4;
    } else {
        // Invalid UTF-8, skip this byte
        utf8_str += 1;
        return 0xFFFD; // Unicode replacement character
    }
    
    return codepoint;
}

void print_at_utf8(int x, int y, const char* utf8_text) {
    if (!g_initialized || !utf8_text) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;

    std::lock_guard<std::mutex> lock(g_text_mutex);
    const char* str = utf8_text;
    int col = x;
    
    while (*str && col < g_text_columns) {
        uint32_t codepoint = decode_utf8_char(str);
        if (codepoint != 0) {
            g_text_buffer[y][col] = codepoint;
            // Set colors for this character position
            g_text_ink_colors[y][col] = g_current_ink_color;
            g_text_paper_colors[y][col] = g_current_paper_color;
            col++;
        }
    }
    g_text_dirty = true;  // Mark text for upload
}

void print_at_no_color(int x, int y, const char* text) {
    if (!g_initialized || !text) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;

    std::lock_guard<std::mutex> lock(g_text_mutex);
    int len = strlen(text);
    int max_chars = g_text_columns - x;
    if (len > max_chars) len = max_chars;
    
    for (int i = 0; i < len; i++) {
        g_text_buffer[y][x + i] = (uint32_t)(unsigned char)text[i];
        // Do NOT change colors - leave existing colors intact
    }
    g_text_dirty = true;  // Mark text for upload
}

void print_at_utf8_no_color(int x, int y, const char* utf8_text) {
    if (!g_initialized || !utf8_text) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;

    std::lock_guard<std::mutex> lock(g_text_mutex);
    const char* str = utf8_text;
    int col = x;
    
    while (*str && col < g_text_columns) {
        uint32_t codepoint = decode_utf8_char(str);
        if (codepoint != 0) {
            g_text_buffer[y][col] = codepoint;
            // Do NOT change colors - leave existing colors intact
            col++;
        }
    }
    g_text_dirty = true;  // Mark text for upload
}

void clear_text() {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    // Clear text buffer (without internal lock since we already have it)
    for (int row = 0; row < g_text_rows; row++) {
        for (int col = 0; col < g_text_columns; col++) {
            g_text_buffer[row][col] = 0x20; // Unicode space
        }
    }
    init_text_colors();
    g_text_dirty = true;  // Mark text for upload
}

void scroll_text(int dx, int dy) {
    // For now, only implement vertical scrolling
    if (dy > 0) {
        for (int i = 0; i < dy; i++) {
            scroll_text_down();
        }
    } else if (dy < 0) {
        for (int i = 0; i < -dy; i++) {
            scroll_text_up();
        }
    }
}

void scroll_text_up() {
    if (!g_initialized) return;
    
    std::lock_guard<std::mutex> lock(g_text_mutex);
    // Move all rows up by one
    for (int row = 0; row < g_text_rows - 1; row++) {
        memcpy(g_text_buffer[row], g_text_buffer[row + 1], g_text_columns * sizeof(uint32_t));
        // Move color maps too
        memcpy(g_text_ink_colors[row], g_text_ink_colors[row + 1], g_text_columns * sizeof(uint32_t));
        memcpy(g_text_paper_colors[row], g_text_paper_colors[row + 1], g_text_columns * sizeof(uint32_t));
    }
    
    // Clear the last row
    for (int col = 0; col < g_text_columns; col++) {
        g_text_buffer[g_text_rows - 1][col] = 0x20; // Unicode space
        g_text_ink_colors[g_text_rows - 1][col] = g_current_ink_color;
        g_text_paper_colors[g_text_rows - 1][col] = g_current_paper_color;
    }
    g_text_dirty = true;  // Mark text for upload
}

void scroll_text_down() {
    if (!g_initialized) return;
    
    std::lock_guard<std::mutex> lock(g_text_mutex);
    // Move all rows down by one
    for (int row = g_text_rows - 1; row > 0; row--) {
        memcpy(g_text_buffer[row], g_text_buffer[row - 1], g_text_columns * sizeof(uint32_t));
        // Move color maps too
        memcpy(g_text_ink_colors[row], g_text_ink_colors[row - 1], g_text_columns * sizeof(uint32_t));
        memcpy(g_text_paper_colors[row], g_text_paper_colors[row - 1], g_text_columns * sizeof(uint32_t));
    }
    
    // Clear the first row
    for (int col = 0; col < g_text_columns; col++) {
        g_text_buffer[0][col] = 0x20; // Unicode space
        g_text_ink_colors[0][col] = g_current_ink_color;
        g_text_paper_colors[0][col] = g_current_paper_color;
    }
    g_text_dirty = true;  // Mark text for upload
}

// Text buffer access functions for screen save/restore
void get_text_buffer_cell(int x, int y, uint32_t* text, uint32_t* ink, uint32_t* paper) {
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) {
        if (text) *text = 0x20; // space
        if (ink) *ink = 0xFFFFFFFF; // white
        if (paper) *paper = 0x000000FF; // black
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_text_mutex);
    if (text) *text = g_text_buffer[y][x];
    if (ink) *ink = g_text_ink_colors[y][x];
    if (paper) *paper = g_text_paper_colors[y][x];
}

void set_text_buffer_cell(int x, int y, uint32_t text, uint32_t ink, uint32_t paper) {
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_text_mutex);
    g_text_buffer[y][x] = text;
    g_text_ink_colors[y][x] = ink;
    g_text_paper_colors[y][x] = paper;
}

static void clear_text_buffer() {
    // Note: This function assumes caller already holds g_text_mutex
    for (int row = 0; row < g_text_rows; row++) {
        for (int col = 0; col < g_text_columns; col++) {
            g_text_buffer[row][col] = 0x20; // Unicode space
        }
    }
}

// Initialize text color maps
static void init_text_colors() {
    for (int row = 0; row < 25; row++) {
        for (int col = 0; col < 80; col++) {
            g_text_ink_colors[row][col] = 0xFFFFFFFF;   // Opaque white ink
            g_text_paper_colors[row][col] = 0x00000000; // Fully transparent by default
        }
    }
    g_current_ink_color = 0xFFFFFFFF;   // Default: opaque white
    g_current_paper_color = 0x00000000; // Default: fully transparent
}

// Pack RGBA components into a 32-bit color
static uint32_t pack_rgba(int r, int g, int b, int a) {
    return ((uint32_t)(r & 0xFF) << 24) | 
           ((uint32_t)(g & 0xFF) << 16) | 
           ((uint32_t)(b & 0xFF) << 8) | 
           ((uint32_t)(a & 0xFF));
}

// Unpack 32-bit color into RGBA components
static void unpack_rgba(uint32_t color, int* r, int* g, int* b, int* a) {
    if (r) *r = (color >> 24) & 0xFF;
    if (g) *g = (color >> 16) & 0xFF;
    if (b) *b = (color >> 8) & 0xFF;
    if (a) *a = color & 0xFF;
}

// =============================================================================
// TEXT COLOR API
// =============================================================================

void set_text_ink(int r, int g, int b, int a) {
    g_current_ink_color = pack_rgba(r, g, b, a);
}

void set_text_paper(int r, int g, int b, int a) {
    g_current_paper_color = pack_rgba(r, g, b, a);
}

// Compatibility function for older code
void set_text_color(int r, int g, int b) {
    set_text_ink(r, g, b, 255);  // Default to opaque
}

// =============================================================================
// CURSOR API
// =============================================================================

void set_cursor_position(int x, int y) {
    if (x >= 0 && x < g_text_columns && y >= 0 && y < g_text_rows) {
        std::lock_guard<std::mutex> lock(g_text_mutex);
        // Only mark dirty if position actually changed
        if (g_text_cursor.x != x || g_text_cursor.y != y) {
            g_text_cursor.x = x;
            g_text_cursor.y = y;
            g_text_dirty = true;
        }
    }
}

void set_cursor_visible(bool visible) {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    // Only mark dirty if visibility actually changed
    if (g_text_cursor.visible != visible) {
        g_text_cursor.visible = visible;
        g_text_dirty = true;
    }
}

void set_cursor_type(int type) {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    if (type >= 0 && type <= 2) {
        // Only mark dirty if type actually changed
        if (g_text_cursor.type != (CursorType)type) {
            g_text_cursor.type = (CursorType)type;
            g_text_dirty = true;
        }
    }
}

void set_cursor_color(int r, int g, int b, int a) {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    uint32_t new_color = pack_rgba(r, g, b, a);
    // Only mark dirty if color actually changed
    if (g_text_cursor.color != new_color) {
        g_text_cursor.color = new_color;
        g_text_dirty = true;
    }
}

void enable_cursor_blink(bool enable) {
    std::lock_guard<std::mutex> lock(g_text_mutex);
    // Only mark dirty if blink setting actually changed
    if (g_text_cursor.blink_enabled != enable) {
        g_text_cursor.blink_enabled = enable;
        g_text_cursor.blink_state = true; // Reset to visible when changing blink state
        g_text_dirty = true;
    }
}

// =============================================================================
// COLOR MAP FILLING FUNCTIONS
// =============================================================================

void fill_color_map(int start_x, int start_y, int width, int height,
                   int ink_r, int ink_g, int ink_b, int ink_a,
                   int paper_r, int paper_g, int paper_b, int paper_a) {
    
    // Bounds checking
    if (start_x < 0 || start_y < 0 || width <= 0 || height <= 0) return;
    if (start_x >= 80 || start_y >= 25) return;
    
    // Clamp to screen boundaries
    int end_x = std::min(start_x + width, 80);
    int end_y = std::min(start_y + height, 25);
    int actual_width = end_x - start_x;
    
    if (actual_width <= 0) return;
    
    // Pack colors once
    uint32_t ink_color = pack_rgba(ink_r, ink_g, ink_b, ink_a);
    uint32_t paper_color = pack_rgba(paper_r, paper_g, paper_b, paper_a);
    
    // Lock the text system
    std::lock_guard<std::mutex> lock(g_text_mutex);
    
    // Fill each row efficiently
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            g_text_ink_colors[y][x] = ink_color;
            g_text_paper_colors[y][x] = paper_color;
        }
    }
    
    // Mark text as dirty to trigger re-render
    g_text_dirty = true;
}

void fill_paper_map(int start_x, int start_y, int width, int height,
                   int paper_r, int paper_g, int paper_b, int paper_a) {
    
    // Bounds checking
    if (start_x < 0 || start_y < 0 || width <= 0 || height <= 0) return;
    if (start_x >= 80 || start_y >= 25) return;
    
    // Clamp to screen boundaries
    int end_x = std::min(start_x + width, 80);
    int end_y = std::min(start_y + height, 25);
    
    if (end_x <= start_x) return;
    
    // Pack color once
    uint32_t paper_color = pack_rgba(paper_r, paper_g, paper_b, paper_a);
    
    // Lock the text system
    std::lock_guard<std::mutex> lock(g_text_mutex);
    
    // Fill each row efficiently
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            g_text_paper_colors[y][x] = paper_color;
        }
    }
    
    // Mark text as dirty to trigger re-render
    g_text_dirty = true;
}

void fill_ink_map(int start_x, int start_y, int width, int height,
                 int ink_r, int ink_g, int ink_b, int ink_a) {
    
    // Bounds checking
    if (start_x < 0 || start_y < 0 || width <= 0 || height <= 0) return;
    if (start_x >= 80 || start_y >= 25) return;
    
    // Clamp to screen boundaries
    int end_x = std::min(start_x + width, 80);
    int end_y = std::min(start_y + height, 25);
    
    if (end_x <= start_x) return;
    
    // Pack color once
    uint32_t ink_color = pack_rgba(ink_r, ink_g, ink_b, ink_a);
    
    // Lock the text system
    std::lock_guard<std::mutex> lock(g_text_mutex);
    
    // Fill each row efficiently
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            g_text_ink_colors[y][x] = ink_color;
        }
    }
    
    // Mark text as dirty to trigger re-render
    g_text_dirty = true;
}

void poke_text_ink(int x, int y, int r, int g, int b, int a) {
    if (!g_initialized) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;
    
    g_text_ink_colors[y][x] = pack_rgba(r, g, b, a);
    g_text_dirty = true;  // Mark text for upload
}

void poke_text_paper(int x, int y, int r, int g, int b, int a) {
    if (!g_initialized) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;
    
    g_text_paper_colors[y][x] = pack_rgba(r, g, b, a);
    g_text_dirty = true;  // Mark text for upload
}

void peek_text_ink(int x, int y, int* r, int* g, int* b, int* a) {
    if (!g_initialized) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;
    
    unpack_rgba(g_text_ink_colors[y][x], r, g, b, a);
}

void peek_text_paper(int x, int y, int* r, int* g, int* b, int* a) {
    if (!g_initialized) return;
    if (x < 0 || y < 0 || x >= g_text_columns || y >= g_text_rows) return;
    
    unpack_rgba(g_text_paper_colors[y][x], r, g, b, a);
}

void fill_text_color(int x, int y, int width, int height, 
                     int ink_r, int ink_g, int ink_b, int ink_a,
                     int paper_r, int paper_g, int paper_b, int paper_a) {
    if (!g_initialized) return;
    
    // Bounds checking
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return;
    if (x >= g_text_columns || y >= g_text_rows) return;
    
    // Clamp to screen bounds
    int end_x = std::min(x + width, g_text_columns);
    int end_y = std::min(y + height, g_text_rows);
    
    // Pack colors once
    uint32_t ink_color = pack_rgba(ink_r, ink_g, ink_b, ink_a);
    uint32_t paper_color = pack_rgba(paper_r, paper_g, paper_b, paper_a);
    
    // Fill the rectangular region
    for (int row = y; row < end_y; row++) {
        // Use memset-style operation for cache efficiency
        for (int col = x; col < end_x; col++) {
            g_text_ink_colors[row][col] = ink_color;
            g_text_paper_colors[row][col] = paper_color;
        }
    }
    
    g_text_dirty = true;  // Mark text for upload
}

void fill_text_ink(int x, int y, int width, int height, int r, int g, int b, int a) {
    if (!g_initialized) return;
    
    // Bounds checking
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return;
    if (x >= g_text_columns || y >= g_text_rows) return;
    
    // Clamp to screen bounds
    int end_x = std::min(x + width, g_text_columns);
    int end_y = std::min(y + height, g_text_rows);
    
    // Pack color once
    uint32_t ink_color = pack_rgba(r, g, b, a);
    
    // Fill the rectangular region
    for (int row = y; row < end_y; row++) {
        for (int col = x; col < end_x; col++) {
            g_text_ink_colors[row][col] = ink_color;
        }
    }
    
    g_text_dirty = true;  // Mark text for upload
}

void fill_text_paper(int x, int y, int width, int height, int r, int g, int b, int a) {
    if (!g_initialized) return;
    
    // Bounds checking
    if (x < 0 || y < 0 || width <= 0 || height <= 0) return;
    if (x >= g_text_columns || y >= g_text_rows) return;
    
    // Clamp to screen bounds
    int end_x = std::min(x + width, g_text_columns);
    int end_y = std::min(y + height, g_text_rows);
    
    // Pack color once
    uint32_t paper_color = pack_rgba(r, g, b, a);
    
    // Fill the rectangular region
    for (int row = y; row < end_y; row++) {
        for (int col = x; col < end_x; col++) {
            g_text_paper_colors[row][col] = paper_color;
        }
    }
    
    g_text_dirty = true;  // Mark text for upload
}

void clear_text_colors(void) {
    if (!g_initialized) return;
    
    uint32_t default_ink = pack_rgba(255, 255, 255, 255);   // White
    uint32_t default_paper = pack_rgba(0, 0, 0, 0);         // Transparent black
    
    // Fill entire screen with default colors
    for (int row = 0; row < g_text_rows; row++) {
        for (int col = 0; col < g_text_columns; col++) {
            g_text_ink_colors[row][col] = default_ink;
            g_text_paper_colors[row][col] = default_paper;
        }
    }
    
    g_text_dirty = true;  // Mark text for upload
}

void wait_for_render_complete(void) {
    if (!g_initialized) return;
    
    // Get current frame count
    uint64_t current_frame = g_frame_counter.load();
    
    // Wait for the next frame to complete
    std::unique_lock<std::mutex> lock(g_frame_sync_mutex);
    g_frame_sync_cv.wait(lock, [current_frame] {
        return g_frame_counter.load() > current_frame;
    });
}

void save_text(int slot) {
    if (!g_initialized) return;
    if (slot < 0) return;
    
    std::lock_guard<std::mutex> lock(g_backup_mutex);
    
    // Expand backup array if needed
    if (slot >= (int)g_text_backups.size()) {
        g_text_backups.resize(slot + 1);
    }
    
    // Copy current screen state to backup slot using efficient bulk operations
    TextBackup& backup = g_text_backups[slot];
    
    // Copy text buffer
    for (int row = 0; row < g_text_rows; row++) {
        memcpy(backup.text_buffer[row], g_text_buffer[row], g_text_columns * sizeof(uint32_t));
        memcpy(backup.ink_colors[row], g_text_ink_colors[row], g_text_columns * sizeof(uint32_t));
        memcpy(backup.paper_colors[row], g_text_paper_colors[row], g_text_columns * sizeof(uint32_t));
    }
}

bool restore_text(int slot) {
    if (!g_initialized) return false;
    if (slot < 0) return false;
    
    std::lock_guard<std::mutex> lock(g_backup_mutex);
    
    // Check if slot exists
    if (slot >= (int)g_text_backups.size()) {
        return false;
    }
    
    // Restore from backup slot
    const TextBackup& backup = g_text_backups[slot];
    
    // Copy backup to current screen state using efficient bulk operations
    for (int row = 0; row < g_text_rows; row++) {
        memcpy(g_text_buffer[row], backup.text_buffer[row], g_text_columns * sizeof(uint32_t));
        memcpy(g_text_ink_colors[row], backup.ink_colors[row], g_text_columns * sizeof(uint32_t));
        memcpy(g_text_paper_colors[row], backup.paper_colors[row], g_text_columns * sizeof(uint32_t));
    }
    
    g_text_dirty = true;  // Mark text for upload
    return true;
}

int get_saved_text_count(void) {
    std::lock_guard<std::mutex> lock(g_backup_mutex);
    return (int)g_text_backups.size();
}

void clear_saved_text(void) {
    std::lock_guard<std::mutex> lock(g_backup_mutex);
    g_text_backups.clear();
}

// =============================================================================
// DISPLAY CONTROL API
// =============================================================================

void set_background_color(int r, int g, int b) {
    g_bg_r.store(r);
    g_bg_g.store(g);
    g_bg_b.store(b);
}

// =============================================================================
// GRAPHICS API IMPLEMENTATION
// =============================================================================

void clear_graphics() {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    
    cairo_save(g_graphics_cr);
    cairo_set_operator(g_graphics_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(g_graphics_cr);
    cairo_restore(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void draw_line(int x1, int y1, int x2, int y2) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_move_to(g_graphics_cr, x1, y1);
    cairo_line_to(g_graphics_cr, x2, y2);
    cairo_stroke(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void draw_rect(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_rectangle(g_graphics_cr, x, y, width, height);
    cairo_stroke(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void fill_rect(int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_rectangle(g_graphics_cr, x, y, width, height);
    cairo_fill(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void draw_circle(int x, int y, int radius) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_arc(g_graphics_cr, x, y, radius, 0, 2 * M_PI);
    cairo_stroke(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void fill_circle(int x, int y, int radius) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_arc(g_graphics_cr, x, y, radius, 0, 2 * M_PI);
    cairo_fill(g_graphics_cr);
    g_graphics_dirty = true;  // Mark graphics for upload
}

void set_draw_color(int r, int g, int b, int a) {
    std::lock_guard<std::mutex> lock(g_graphics_mutex);
    if (!g_graphics_cr) return;
    cairo_set_source_rgba(g_graphics_cr, r/255.0, g/255.0, b/255.0, a/255.0);
    // Note: Setting color alone doesn't require upload, only when drawing
}

// =============================================================================

int get_screen_width() {
    return g_screen_width;
}

int get_screen_height() {
    return g_screen_height;
}

// =============================================================================
// CLEANUP
// =============================================================================

static void cleanup_all() {
    // Cleanup sprite system
    if (g_sprite_renderer) {
        g_sprite_renderer->shutdown();
        delete g_sprite_renderer;
        g_sprite_renderer = nullptr;
    }
    if (g_sprite_bank) {
        g_sprite_bank->shutdown();
        delete g_sprite_bank;
        g_sprite_bank = nullptr;
    }
    
    // Cleanup front tile system
    if (g_tile_cr) {
        cairo_destroy(g_tile_cr);
        g_tile_cr = nullptr;
    }
    if (g_tile_surface) {
        cairo_surface_destroy(g_tile_surface);
        g_tile_surface = nullptr;
    }
    if (g_tile_bitmap) {
        g_tile_bitmap = nullptr; // Points to Cairo surface data, don't delete
    }
    if (g_world_map) {
        delete[] g_world_map;
        g_world_map = nullptr;
    }
    
    // Cleanup back tile system
    if (g_back_tile_cr) {
        cairo_destroy(g_back_tile_cr);
        g_back_tile_cr = nullptr;
    }
    if (g_back_tile_surface) {
        cairo_surface_destroy(g_back_tile_surface);
        g_back_tile_surface = nullptr;
    }
    if (g_back_tile_bitmap) {
        g_back_tile_bitmap = nullptr; // Points to Cairo surface data, don't delete
    }
    if (g_back_world_map) {
        delete[] g_back_world_map;
        g_back_world_map = nullptr;
    }
    // Cleanup loaded tile surfaces
    for (int i = 0; i < 256; i++) {
        if (g_tile_surfaces[i]) {
            cairo_surface_destroy(g_tile_surfaces[i]);
            g_tile_surfaces[i] = nullptr;
        }
    }
    if (g_tile_texture) {
        glDeleteTextures(1, &g_tile_texture);
    }
    if (g_back_tile_texture) {
        glDeleteTextures(1, &g_back_tile_texture);
    }
    g_sprites_initialized = false;

    if (g_graphics_texture) {
        glDeleteTextures(1, &g_graphics_texture);
    }
    if (g_graphics_cr) {
        cairo_destroy(g_graphics_cr);
    }
    if (g_graphics_surface) {
        cairo_surface_destroy(g_graphics_surface);
    }
    if (g_graphics_bitmap) {
        delete[] g_graphics_bitmap;
    }

    if (g_text_texture) {
        glDeleteTextures(1, &g_text_texture);
    }
    if (g_text_bitmap) {
        delete[] g_text_bitmap;
    }

    if (g_ft_face) {
        FT_Done_Face(g_ft_face);
    }
    if (g_ft_library) {
        FT_Done_FreeType(g_ft_library);
    }

    if (g_context) {
        SDL_GL_DeleteContext(g_context);
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
    }

    SDL_Quit();
}

// =============================================================================
// API IMPLEMENTATIONS
// =============================================================================

// =============================================================================
// SPRITE API IMPLEMENTATIONS  
// =============================================================================

bool init_sprites() {
    if (g_sprites_initialized) {
        return true;
    }
    
    return init_sprite_system();
}

bool load_sprite(int slot, const char* filename) {
    if (!g_sprite_bank) {
        return false;
    }
    
    return g_sprite_bank->load_sprite(slot, std::string(filename));
}

bool sprite(int instance_id, int sprite_slot, int x, int y) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite(instance_id, sprite_slot, (float)x, (float)y);
}

bool sprite_move(int instance_id, int x, int y) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_move(instance_id, (float)x, (float)y);
}

bool sprite_scale(int instance_id, float scale_x, float scale_y) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_scale(instance_id, scale_x, scale_y);
}

bool sprite_rotate(int instance_id, float degrees) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_rotate(instance_id, degrees);
}

bool sprite_alpha(int instance_id, float alpha) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_alpha(instance_id, alpha);
}

bool sprite_z_order(int instance_id, int z_order) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_z_order(instance_id, z_order);
}

bool sprite_hide(int instance_id) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_hide(instance_id);
}

bool sprite_show(int instance_id) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_show(instance_id);
}

bool sprite_is_visible(int instance_id) {
    if (!g_sprite_renderer) {
        return false;
    }
    
    return g_sprite_renderer->sprite_is_visible(instance_id);
}

void hide_all_sprites() {
    if (g_sprite_renderer) {
        g_sprite_renderer->hide_all_sprites();
    }
}

int get_active_sprite_count() {
    if (!g_sprite_renderer) {
        return 0;
    }
    
    return g_sprite_renderer->get_active_count();
}

// =============================================================================
// TILE SYSTEM API
// =============================================================================

bool init_tiles() {
    if (g_tiles_initialized) {
        return true;
    }
    
    // Debug: Print screen dimensions
    std::cout << "[Runtime] init_tiles() called with screen: " << g_screen_width << "x" << g_screen_height << std::endl;
    
    // Calculate tile view dimensions (screen + 3 tile border on all sides)
    g_tile_view_width = g_screen_width + 768;   // +384 left + 384 right
    g_tile_view_height = g_screen_height + 768; // +384 top + 384 bottom
    
    // Calculate viewport grid size to fill the view
    g_tile_grid_width = (g_tile_view_width + 127) / 128;   // Ceiling division
    g_tile_grid_height = (g_tile_view_height + 127) / 128; // Ceiling division
    
    std::cout << "[Runtime] Calculated grid: " << g_tile_grid_width << "x" << g_tile_grid_height << std::endl;
    
    // Initialize front layer world map size (same as viewport if not set)
    if (g_world_map_width == 0 || g_world_map_height == 0) {
        g_world_map_width = g_tile_grid_width;
        g_world_map_height = g_tile_grid_height;
    }
    
    // Initialize back layer world map size (same as front if not set)
    if (g_back_world_map_width == 0 || g_back_world_map_height == 0) {
        g_back_world_map_width = g_world_map_width;
        g_back_world_map_height = g_world_map_height;
    }
    
    // Allocate front layer world map
    if (!g_world_map) {
        g_world_map = new int[g_world_map_width * g_world_map_height];
        memset(g_world_map, 0, g_world_map_width * g_world_map_height * sizeof(int));
    }
    
    // Allocate back layer world map
    if (!g_back_world_map) {
        g_back_world_map = new int[g_back_world_map_width * g_back_world_map_height];
        memset(g_back_world_map, 0, g_back_world_map_width * g_back_world_map_height * sizeof(int));
    }
    
    // Create Cairo surface for front tile composition
    g_tile_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, g_tile_view_width, g_tile_view_height);
    if (cairo_surface_status(g_tile_surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Runtime] Failed to create front tile Cairo surface" << std::endl;
        delete[] g_world_map;
        delete[] g_back_world_map;
        g_world_map = nullptr;
        g_back_world_map = nullptr;
        return false;
    }
    
    g_tile_cr = cairo_create(g_tile_surface);
    if (cairo_status(g_tile_cr) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Runtime] Failed to create front tile Cairo context" << std::endl;
        cairo_surface_destroy(g_tile_surface);
        g_tile_surface = nullptr;
        delete[] g_world_map;
        delete[] g_back_world_map;
        g_world_map = nullptr;
        g_back_world_map = nullptr;
        return false;
    }
    
    // Create Cairo surface for back tile composition
    g_back_tile_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, g_tile_view_width, g_tile_view_height);
    if (cairo_surface_status(g_back_tile_surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Runtime] Failed to create back tile Cairo surface" << std::endl;
        cairo_destroy(g_tile_cr);
        cairo_surface_destroy(g_tile_surface);
        g_tile_cr = nullptr;
        g_tile_surface = nullptr;
        delete[] g_world_map;
        delete[] g_back_world_map;
        g_world_map = nullptr;
        g_back_world_map = nullptr;
        return false;
    }
    
    g_back_tile_cr = cairo_create(g_back_tile_surface);
    if (cairo_status(g_back_tile_cr) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Runtime] Failed to create back tile Cairo context" << std::endl;
        cairo_surface_destroy(g_back_tile_surface);
        cairo_destroy(g_tile_cr);
        cairo_surface_destroy(g_tile_surface);
        g_back_tile_surface = nullptr;
        g_tile_cr = nullptr;
        g_tile_surface = nullptr;
        delete[] g_world_map;
        delete[] g_back_world_map;
        g_world_map = nullptr;
        g_back_world_map = nullptr;
        return false;
    }
    
    // Get pointers to Cairo surface data
    g_tile_bitmap = cairo_image_surface_get_data(g_tile_surface);
    g_back_tile_bitmap = cairo_image_surface_get_data(g_back_tile_surface);
    
    g_tiles_initialized = true;
    std::cout << "[Runtime] Dual tile system initialized successfully" << std::endl;
    std::cout << "[Runtime] Viewport: " << g_tile_grid_width << "x" << g_tile_grid_height << " tiles" << std::endl;
    std::cout << "[Runtime] Front layer world map: " << g_world_map_width << "x" << g_world_map_height << " tiles" << std::endl;
    std::cout << "[Runtime] Back layer world map: " << g_back_world_map_width << "x" << g_back_world_map_height << " tiles" << std::endl;
    
    // Final debug check
    if (g_world_map_width == 0 || g_world_map_height == 0) {
        std::cerr << "[Runtime] ERROR: World map size is 0! Screen was " << g_screen_width << "x" << g_screen_height << std::endl;
    }
    return true;
}

bool load_tile(int tile_id, const char* filename) {
    if (!g_tiles_initialized) {
        return false;
    }
    
    if (tile_id <= 0 || tile_id >= 256) {
        std::cerr << "[Runtime] Invalid tile ID " << tile_id << std::endl;
        return false;
    }
    
    // Load PNG directly with Cairo
    cairo_surface_t* surface = cairo_image_surface_create_from_png(filename);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Runtime] Failed to load tile PNG: " << filename << std::endl;
        cairo_surface_destroy(surface);
        return false;
    }
    
    // Free existing tile surface if any
    if (g_tile_surfaces[tile_id]) {
        cairo_surface_destroy(g_tile_surfaces[tile_id]);
    }
    
    g_tile_surfaces[tile_id] = surface;
    g_tile_dirty = true;  // Mark for rebuild
    
    std::cout << "[Runtime] Loaded tile " << tile_id << " from " << filename << std::endl;
    return true;
}

void set_tile(int world_x, int world_y, int tile_id) {
    if (!g_tiles_initialized || !g_world_map) {
        return;
    }
    
    if (world_x >= 0 && world_x < g_world_map_width && 
        world_y >= 0 && world_y < g_world_map_height) {
        g_world_map[world_y * g_world_map_width + world_x] = tile_id;
        g_tile_dirty = true;  // Mark for rebuild
    }
}

int get_tile(int world_x, int world_y) {
    if (!g_tiles_initialized || !g_world_map) {
        return 0;
    }
    
    if (world_x >= 0 && world_x < g_world_map_width && 
        world_y >= 0 && world_y < g_world_map_height) {
        return g_world_map[world_y * g_world_map_width + world_x];
    }
    return 0;
}

void fill_tiles(int start_x, int start_y, int width, int height, int tile_id) {
    if (!g_tiles_initialized || !g_world_map) {
        return;
    }
    
    for (int y = start_y; y < start_y + height && y < g_world_map_height; y++) {
        for (int x = start_x; x < start_x + width && x < g_world_map_width; x++) {
            if (x >= 0 && y >= 0) {
                g_world_map[y * g_world_map_width + x] = tile_id;
            }
        }
    }
    g_tile_dirty = true;  // Mark for rebuild
}

void clear_tiles() {
    if (!g_tiles_initialized || !g_world_map) {
        return;
    }
    
    memset(g_world_map, 0, g_world_map_width * g_world_map_height * sizeof(int));
    g_tile_dirty = true;  // Mark for rebuild
}

void scroll_tiles(float dx, float dy) {
    if (!g_tiles_initialized) {
        return;
    }
    
    g_tile_scroll_x += dx;
    g_tile_scroll_y += dy;
    
    // Update viewport position for world map scrolling
    g_viewport_x += dx;
    g_viewport_y += dy;
    
    // Handle viewport shifting when exceeding ±128 pixels, maintaining visual continuity
    bool viewport_shifted = false;
    
    // Handle horizontal viewport shifting (3 tile border = ±384 pixels)
    if (g_tile_scroll_x > 384.0f) {
        g_tile_scroll_x -= 384.0f;
        viewport_shifted = true;
    } else if (g_tile_scroll_x < -384.0f) {
        g_tile_scroll_x += 384.0f;
        viewport_shifted = true;
    }
    
    // Handle vertical viewport shifting (3 tile border = ±384 pixels)
    if (g_tile_scroll_y > 384.0f) {
        g_tile_scroll_y -= 384.0f;
        viewport_shifted = true;
    } else if (g_tile_scroll_y < -384.0f) {
        g_tile_scroll_y += 384.0f;
        viewport_shifted = true;
    }
    
    // If viewport shifted, mark tiles for regeneration
    if (viewport_shifted) {
        g_tile_dirty = true;
    }
}

void set_tile_scroll(float x, float y) {
    if (!g_tiles_initialized) {
        return;
    }
    
    // Store previous viewport tile position
    int old_tile_x = (int)(g_viewport_x / 128.0f);
    int old_tile_y = (int)(g_viewport_y / 128.0f);
    
    // Set absolute viewport position
    g_viewport_x = x;
    g_viewport_y = y;
    
    // Calculate which tile the viewport should be centered on
    int new_tile_x = (int)(g_viewport_x / 128.0f);
    int new_tile_y = (int)(g_viewport_y / 128.0f);
    
    // Calculate sub-tile offset within the current tile
    g_tile_scroll_x = g_viewport_x - (new_tile_x * 128.0f);
    g_tile_scroll_y = g_viewport_y - (new_tile_y * 128.0f);
    
    // Check if viewport has shifted to a different tile
    if (old_tile_x != new_tile_x || old_tile_y != new_tile_y) {
        g_tile_dirty = true;  // Viewport shifted, regenerate texture
    }
}

bool set_world_map_size(int width, int height) {
    if (g_tiles_initialized) {
        std::cerr << "[Runtime] Cannot set world map size after tiles are initialized" << std::endl;
        return false;
    }
    
    if (width <= 0 || height <= 0) {
        std::cerr << "[Runtime] Invalid world map size: " << width << "x" << height << std::endl;
        return false;
    }
    
    // Clean up existing world map if any
    if (g_world_map) {
        delete[] g_world_map;
        g_world_map = nullptr;
    }
    
    g_world_map_width = width;
    g_world_map_height = height;
    
    std::cout << "[Runtime] World map size set to " << width << "x" << height << " tiles" << std::endl;
    return true;
}

void get_world_map_size(int* width, int* height) {
    if (!width || !height) {
        return;
    }
    
    *width = g_world_map_width;
    *height = g_world_map_height;
}

void get_tile_grid_size(int* width, int* height) {
    if (!g_tiles_initialized || !width || !height) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    
    *width = g_tile_grid_width;  // Viewport size, not world map size
    *height = g_tile_grid_height;
}

// =============================================================================
// BACK TILE LAYER API (LAYER_TILES2) - Parallax Support
// =============================================================================

void set_back_tile(int world_x, int world_y, int tile_id) {
    if (!g_tiles_initialized || !g_back_world_map) {
        return;
    }
    
    if (world_x >= 0 && world_x < g_back_world_map_width && 
        world_y >= 0 && world_y < g_back_world_map_height) {
        g_back_world_map[world_y * g_back_world_map_width + world_x] = tile_id;
        g_back_tile_dirty = true;  // Mark for rebuild
    }
}

int get_back_tile(int world_x, int world_y) {
    if (!g_tiles_initialized || !g_back_world_map) {
        return 0;
    }
    
    if (world_x >= 0 && world_x < g_back_world_map_width && 
        world_y >= 0 && world_y < g_back_world_map_height) {
        return g_back_world_map[world_y * g_back_world_map_width + world_x];
    }
    return 0;
}

void fill_back_tiles(int start_x, int start_y, int width, int height, int tile_id) {
    if (!g_tiles_initialized || !g_back_world_map) {
        return;
    }
    
    for (int y = start_y; y < start_y + height && y < g_back_world_map_height; y++) {
        for (int x = start_x; x < start_x + width && x < g_back_world_map_width; x++) {
            if (x >= 0 && y >= 0) {
                g_back_world_map[y * g_back_world_map_width + x] = tile_id;
            }
        }
    }
    g_back_tile_dirty = true;  // Mark for rebuild
}

void clear_back_tiles() {
    if (!g_tiles_initialized || !g_back_world_map) {
        return;
    }
    
    memset(g_back_world_map, 0, g_back_world_map_width * g_back_world_map_height * sizeof(int));
    g_back_tile_dirty = true;  // Mark for rebuild
}

void scroll_back_tiles(float dx, float dy) {
    if (!g_tiles_initialized) {
        return;
    }
    
    g_back_tile_scroll_x += dx;
    g_back_tile_scroll_y += dy;
    
    // Update viewport position for world map scrolling
    g_back_viewport_x += dx;
    g_back_viewport_y += dy;
    
    // Handle viewport shifting when exceeding ±128 pixels, maintaining visual continuity
    bool viewport_shifted = false;
    
    // Handle horizontal viewport shifting (3 tile border = ±384 pixels)
    if (g_back_tile_scroll_x > 384.0f) {
        g_back_tile_scroll_x -= 384.0f;
        viewport_shifted = true;
    } else if (g_back_tile_scroll_x < -384.0f) {
        g_back_tile_scroll_x += 384.0f;
        viewport_shifted = true;
    }
    
    // Handle vertical viewport shifting (3 tile border = ±384 pixels)
    if (g_back_tile_scroll_y > 384.0f) {
        g_back_tile_scroll_y -= 384.0f;
        viewport_shifted = true;
    } else if (g_back_tile_scroll_y < -384.0f) {
        g_back_tile_scroll_y += 384.0f;
        viewport_shifted = true;
    }
    
    // If viewport shifted, mark tiles for regeneration
    if (viewport_shifted) {
        g_back_tile_dirty = true;
    }
}

void set_back_tile_scroll(float x, float y) {
    if (!g_tiles_initialized) {
        return;
    }
    
    // Store previous viewport tile position
    int old_tile_x = (int)(g_back_viewport_x / 128.0f);
    int old_tile_y = (int)(g_back_viewport_y / 128.0f);
    
    // Set absolute viewport position
    g_back_viewport_x = x;
    g_back_viewport_y = y;
    
    // Calculate which tile the viewport should be centered on
    int new_tile_x = (int)(g_back_viewport_x / 128.0f);
    int new_tile_y = (int)(g_back_viewport_y / 128.0f);
    
    // Calculate sub-tile offset within the current tile
    g_back_tile_scroll_x = g_back_viewport_x - (new_tile_x * 128.0f);
    g_back_tile_scroll_y = g_back_viewport_y - (new_tile_y * 128.0f);
    
    // Check if viewport has shifted to a different tile
    if (old_tile_x != new_tile_x || old_tile_y != new_tile_y) {
        g_back_tile_dirty = true;  // Viewport shifted, regenerate texture
    }
}

void get_back_tile_scroll(float* x, float* y) {
    if (!x || !y) {
        return;
    }
    
    *x = g_back_viewport_x;
    *y = g_back_viewport_y;
}

void get_tile_scroll(float* x, float* y) {
    if (!x || !y) {
        return;
    }

    *x = g_viewport_x;
    *y = g_viewport_y;
}

// =============================================================================
// LUAJIT MULTI-THREADING C API WRAPPERS
// =============================================================================

int exec_lua(const char* filepath) {
    if (!filepath) return -1;
    
    auto thread_handle = LuaThreadManager::getInstance().exec_lua(std::string(filepath));
    if (thread_handle) {
        return thread_handle->thread_id;
    }
    return -1;
}

int exec_lua_string(const char* script, const char* name) {
    if (!script) return -1;
    
    std::string script_str(script);
    std::string name_str = name ? std::string(name) : "inline";
    
    auto thread_handle = LuaThreadManager::getInstance().exec_lua_string(script_str, name_str);
    if (thread_handle) {
        return thread_handle->thread_id;
    }
    return -1;
}

bool stop_lua_thread(int thread_id) {
    auto threads = LuaThreadManager::getInstance().get_active_threads();
    for (auto& thread : threads) {
        if (thread->thread_id == thread_id) {
            return LuaThreadManager::getInstance().stop_lua_thread(thread);
        }
    }
    return false;
}

void stop_all_lua_threads() {
    LuaThreadManager::getInstance().stop_all_threads();
}

int get_lua_thread_count() {
    return LuaThreadManager::getInstance().get_thread_count();
}

// =============================================================================
// CONSOLE OUTPUT API IMPLEMENTATION
// =============================================================================

#include <cstdarg>
#include <cstdio>
#include <iostream>

void console_print(const char* message) {
    std::cout << message << std::endl;
}

void console_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    std::cout << std::endl;
}

void console_info(const char* message) {
    std::cout << "[INFO] " << message << std::endl;
}

void console_warning(const char* message) {
    std::cout << "[WARNING] " << message << std::endl;
}

void console_error(const char* message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void console_debug(const char* message) {
#ifdef DEBUG
    std::cout << "[DEBUG] " << message << std::endl;
#endif
}

void console_section(const char* section_name) {
    std::cout << "\n=== " << section_name << " ===" << std::endl;
}

void console_separator() {
    std::cout << "----------------------------------------" << std::endl;
}

// =============================================================================
// ENHANCED KEY DETECTION
// =============================================================================

int any_key_pressed() {
    // First try key() for ASCII characters and basic special keys
    int ascii_key = key();
    if (ascii_key != -1) {
        return ascii_key;
    }
    
    // Check extended keys that don't have ASCII equivalents
    if (is_key_pressed(INPUT_KEY_LEFT)) return INPUT_KEY_LEFT;
    if (is_key_pressed(INPUT_KEY_RIGHT)) return INPUT_KEY_RIGHT;
    if (is_key_pressed(INPUT_KEY_UP)) return INPUT_KEY_UP;
    if (is_key_pressed(INPUT_KEY_DOWN)) return INPUT_KEY_DOWN;
    if (is_key_pressed(INPUT_KEY_HOME)) return INPUT_KEY_HOME;
    if (is_key_pressed(INPUT_KEY_END)) return INPUT_KEY_END;
    if (is_key_pressed(INPUT_KEY_PAGE_UP)) return INPUT_KEY_PAGE_UP;
    if (is_key_pressed(INPUT_KEY_PAGE_DOWN)) return INPUT_KEY_PAGE_DOWN;
    if (is_key_pressed(INPUT_KEY_INSERT)) return INPUT_KEY_INSERT;
    
    // Function keys
    if (is_key_pressed(INPUT_KEY_F1)) return INPUT_KEY_F1;
    if (is_key_pressed(INPUT_KEY_F2)) return INPUT_KEY_F2;
    if (is_key_pressed(INPUT_KEY_F3)) return INPUT_KEY_F3;
    if (is_key_pressed(INPUT_KEY_F4)) return INPUT_KEY_F4;
    if (is_key_pressed(INPUT_KEY_F5)) return INPUT_KEY_F5;
    if (is_key_pressed(INPUT_KEY_F6)) return INPUT_KEY_F6;
    if (is_key_pressed(INPUT_KEY_F7)) return INPUT_KEY_F7;
    if (is_key_pressed(INPUT_KEY_F8)) return INPUT_KEY_F8;
    if (is_key_pressed(INPUT_KEY_F9)) return INPUT_KEY_F9;
    if (is_key_pressed(INPUT_KEY_F10)) return INPUT_KEY_F10;
    if (is_key_pressed(INPUT_KEY_F11)) return INPUT_KEY_F11;
    if (is_key_pressed(INPUT_KEY_F12)) return INPUT_KEY_F12;
    
    // Modifier keys
    if (is_key_pressed(INPUT_KEY_LSHIFT)) return INPUT_KEY_LSHIFT;
    if (is_key_pressed(INPUT_KEY_RSHIFT)) return INPUT_KEY_RSHIFT;
    if (is_key_pressed(INPUT_KEY_LCTRL)) return INPUT_KEY_LCTRL;
    if (is_key_pressed(INPUT_KEY_RCTRL)) return INPUT_KEY_RCTRL;
    if (is_key_pressed(INPUT_KEY_LALT)) return INPUT_KEY_LALT;
    if (is_key_pressed(INPUT_KEY_RALT)) return INPUT_KEY_RALT;
    if (is_key_pressed(INPUT_KEY_LGUI)) return INPUT_KEY_LGUI;
    if (is_key_pressed(INPUT_KEY_RGUI)) return INPUT_KEY_RGUI;
    
    // No key currently pressed
    return -1;
}

