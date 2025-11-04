#ifndef ABSTRACT_RUNTIME_H
#define ABSTRACT_RUNTIME_H

#include <cstdint>
#include <vector>
#include <string>



// =============================================================================
// SCREEN MODE CONSTANTS
// =============================================================================

constexpr int SCREEN_20_COLUMN = 0;   // 20 columns × 14px + margins = ~320x400
constexpr int SCREEN_40_COLUMN = 1;   // 40 columns × 14px + margins = ~600x500  
constexpr int SCREEN_80_COLUMN = 2;   // 80 columns × 14px + margins = ~1160x700

// Legacy constants for compatibility
constexpr int SCREEN_640x480 = 0;
constexpr int SCREEN_800x600 = 1; 
constexpr int SCREEN_1920x1080 = 2;

// =============================================================================
// INPUT CONSTANTS
// =============================================================================

// Mouse button constants
constexpr uint32_t MOUSE_LEFT = 1;
constexpr uint32_t MOUSE_RIGHT = 2;
constexpr uint32_t MOUSE_MIDDLE = 4;

// Key code constants
constexpr int KEYCODE_SPACE = 32;
constexpr int KEYCODE_ENTER = 13;
constexpr int KEYCODE_ESC = 27;
constexpr int KEYCODE_BACKSPACE = 8;
constexpr int KEYCODE_DELETE = 127;
constexpr int KEYCODE_LEFT = 200;
constexpr int KEYCODE_RIGHT = 201;
constexpr int KEYCODE_UP = 202;
constexpr int KEYCODE_DOWN = 203;
constexpr int KEYCODE_HOME = 204;
constexpr int KEYCODE_END = 205;

// =============================================================================
// COLOR CONSTANTS
// =============================================================================

constexpr uint32_t COLOR_BLACK = 0x000000FF;
constexpr uint32_t COLOR_WHITE = 0xFFFFFFFF;
constexpr uint32_t COLOR_RED = 0xFF0000FF;
constexpr uint32_t COLOR_GREEN = 0x00FF00FF;
constexpr uint32_t COLOR_BLUE = 0x0000FFFF;
constexpr uint32_t COLOR_CYAN = 0x00FFFFFF;
constexpr uint32_t COLOR_MAGENTA = 0xFF00FFFF;
constexpr uint32_t COLOR_YELLOW = 0xFFFF00FF;
constexpr uint32_t COLOR_GRAY = 0x808080FF;
constexpr uint32_t COLOR_TRANSPARENT = 0x00000000;

// =============================================================================
// CORE RUNTIME MANAGEMENT
// =============================================================================

/**
 * Initialize the abstract runtime system
 * @param screen_mode One of SCREEN_20_COLUMN, SCREEN_40_COLUMN, SCREEN_80_COLUMN (or legacy constants)
 * @return true on success, false on failure
 */
bool init_abstract_runtime(int screen_mode);

/**
 * Shutdown the abstract runtime system
 */
void shutdown_abstract_runtime();

/**
 * Request runtime exit from user application
 * This signals the main thread to cleanly shut down and exit.
 * Safe to call from background thread.
 */
void exit_runtime();

/**
 * Check if runtime quit was requested (e.g., user clicked window close button)
 * This allows user applications to check for quit requests and exit gracefully.
 * Safe to call from any thread.
 * @return true if quit was requested, false otherwise
 */
bool should_quit();

/**
 * Disable automatic quit on SDL events (like window close)
 * After calling this, only explicit exit_runtime() calls will quit the runtime
 */
void disable_auto_quit();

/**
 * Enable automatic quit on SDL events (default behavior)
 * Allows window close button and system quit events to exit the runtime
 */
void enable_auto_quit();

/**
 * Run the main event loop (blocking call)
 * Must be called from the main thread after init_abstract_runtime().
 * @return exit code (0 for normal exit)
 */
int run_main_loop();

/**
 * Run the runtime with a user application function on background thread
 * This is the recommended way to use the runtime.
 * 
 * @param screen_mode Screen mode constant (SCREEN_20_COLUMN, etc. or legacy constants)
 * @param user_app_func User application function to run on background thread
 * @return exit code (0 for normal exit)
 */
int run_runtime_with_app(int screen_mode, void (*user_app_func)(void));

// =============================================================================
// HIGH-LEVEL TEXT INPUT API
// =============================================================================

/**
 * Simple text input at specified screen position
 * This function blocks until the user presses Enter or Escape
 * The runtime handles all text editing internally - no event loop required in app
 * 
 * @param x Column position (0-based)
 * @param y Row position (0-based) 
 * @param max_length Maximum input length (0 = use default)
 * @return Entered text (caller must free), or NULL if cancelled
 */
char* accept_at(int x, int y, int max_length = 50);

/**
 * Text input with prompt at specified position
 * 
 * @param x Column position for prompt (0-based)
 * @param y Row position (0-based)
 * @param prompt Text to display before input field
 * @param max_length Maximum input length (0 = use default)
 * @return Entered text (caller must free), or NULL if cancelled
 */
char* accept_at_with_prompt(int x, int y, const char* prompt, int max_length = 50);

/**
 * Password input (shows asterisks instead of characters)
 * 
 * @param x Column position (0-based)
 * @param y Row position (0-based)
 * @param max_length Maximum input length (0 = use default)
 * @return Entered text (caller must free), or NULL if cancelled
 */
char* accept_password_at(int x, int y, int max_length = 50);

// =============================================================================
// CHARACTER INPUT API (RDCH - Read Character)
// =============================================================================

/**
 * Read a single character from the terminal (blocks until available)
 * This is the classic RDCH function - like getchar() but integrated with runtime
 * 
 * @return Character typed by user, or 0 if runtime is shutting down
 */
char rdch();

/**
 * Read a character if available, otherwise return immediately
 * 
 * @return Character if available, 0 if no character waiting
 */
char rdch_nowait();

/**
 * Check if a character is available for reading
 * 
 * @return true if rdch_nowait() would return a character
 */
bool char_available();

/**
 * Check if any key is currently pressed (immediate state check)
 * This is different from rdch - it checks physical key state, not buffered input
 * 
 * @return Character if key currently pressed, -1 if no key pressed
 */
int key();

/**
 * Runtime text input update functions (called internally by runtime main loop)
 * Applications should not call these directly
 */
void update_runtime_text_input();
void process_runtime_text_input_events();

// =============================================================================
// SCREEN EDITOR API
// =============================================================================

/**
 * Screen editor for multi-line text editing within a defined area
 * Similar to editors on retro computers - allows full editing of text within
 * a rectangular screen region. Supports insert, delete, backspace, arrow keys.
 * 
 * @param x Starting column position (0-based)
 * @param y Starting row position (0-based) 
 * @param width Width of editing area in characters
 * @param height Height of editing area in characters
 * @param initial_text Initial text content (can be multi-line, NULL for empty)
 * @return Edited text content (caller must free), or NULL if cancelled
 * 
 * Controls:
 * - Arrow keys: Move cursor
 * - Insert/Delete/Backspace: Edit text
 * - Enter: New line
 * - F10: Complete editing and return text
 * - Escape: Cancel editing
 */
char* screen_editor(int x, int y, int width, int height, const char* initial_text = nullptr);

/**
 * Runtime screen editor update functions (called internally by runtime main loop)
 * Applications should not call these directly
 */
void update_runtime_screen_editor();
void process_runtime_screen_editor_events();

// =============================================================================
// LUAJIT MULTI-THREADING API
// =============================================================================

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
 * @return Thread ID for the new Lua thread, or -1 on error
 */
int exec_lua(const char* filepath);

/**
 * Execute a Lua script string in a new thread
 * @param script Lua script content
 * @param name Optional name for the script (for debugging)
 * @return Thread ID for the new Lua thread, or -1 on error
 */
int exec_lua_string(const char* script, const char* name = "inline");

/**
 * Stop a running Lua thread
 * @param thread_id Thread ID to stop
 * @return true if thread was stopped, false if already finished or error
 */
bool stop_lua_thread(int thread_id);

/**
 * Stop all running Lua threads
 */
void stop_all_lua_threads();

/**
 * Get the number of currently running Lua threads
 * @return Number of active threads
 */
int get_lua_thread_count();

// =============================================================================
// LUA REPL OVERLAY API
// =============================================================================

/**
 * Runtime Lua REPL overlay system
 * The REPL runs as a background thread and can be toggled on/off with hotkeys.
 * Default hotkeys: F8 = Show REPL, F9 = Hide REPL
 * The overlay appears over any running application without interrupting it.
 */

/**
 * Initialize the Lua REPL overlay system
 * Must be called during runtime initialization
 * @return true if successful, false on error
 */
bool init_repl_overlay();

/**
 * Shutdown the Lua REPL overlay system
 * Cleans up the background thread and resources
 */
void shutdown_repl_overlay();

/**
 * Toggle the visibility of the Lua REPL overlay
 * @param visible true to show, false to hide
 */
void set_repl_overlay_visible(bool visible);

/**
 * Check if the Lua REPL overlay is currently visible
 * @return true if visible, false if hidden
 */
bool is_repl_overlay_visible();

/**
 * Runtime Lua REPL overlay update functions (called internally by runtime main loop)
 * Applications should not call these directly
 */
void process_repl_overlay_hotkeys();

// =============================================================================
// DISPLAY CONTROL
// =============================================================================

/**
 * Set background color
 * @param r Red component (0-255)
 * @param g Green component (0-255) 
 * @param b Blue component (0-255)
 */
void set_background_color(int r, int g, int b);

/**
 * Get screen width in pixels
 * @return screen width
 */
int get_screen_width();

/**
 * Get screen height in pixels  
 * @return screen height
 */
int get_screen_height();

// =============================================================================
// TEXT SYSTEM
// =============================================================================

/**
 * Print text at specific grid coordinates
 * @param x Text column
 * @param y Text row
 * @param text Text string to print
 */
void print_at(int x, int y, const char* text);

/**
 * Print UTF-8 text at specified position (Unicode-aware)
 * 
 * @param x Column position (0-based)
 * @param y Row position (0-based) 
 * @param text UTF-8 encoded text string
 */
void print_at_utf8(int x, int y, const char* utf8_text);

/**
 * Print text at specific grid coordinates without changing colors
 * Uses existing ink/paper colors at each position
 * @param x Text column
 * @param y Text row
 * @param text Text string to print
 */
void print_at_no_color(int x, int y, const char* text);

/**
 * Print UTF-8 text without changing colors
 * Uses existing ink/paper colors at each position
 * @param x Text column
 * @param y Text row
 * @param text UTF-8 encoded text string
 */
void print_at_utf8_no_color(int x, int y, const char* utf8_text);

/**
 * Get text buffer cell contents for screen save/restore
 */
void get_text_buffer_cell(int x, int y, uint32_t* text, uint32_t* ink, uint32_t* paper);

/**
 * Set text buffer cell contents for screen save/restore
 */
void set_text_buffer_cell(int x, int y, uint32_t text, uint32_t ink, uint32_t paper);

/**
 * Clear the entire text screen
 */
void clear_text();

/**
 * Scroll text content by pixel offset
 * @param dx Horizontal pixel offset (not implemented)
 * @param dy Vertical pixel offset (positive = down, negative = up)
 */
void scroll_text(int dx, int dy);

/**
 * Scroll text up by one line (efficient memcpy-based)
 * Top line disappears, bottom line becomes empty
 */
void scroll_text_up();

/**
 * Scroll text down by one line (efficient memcpy-based)  
 * Bottom line disappears, top line becomes empty
 */
void scroll_text_down();

/**
 * Set current text foreground (ink) color for new text
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void set_text_ink(int r, int g, int b, int a);

/**
 * Set text background (paper) color for new text
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void set_text_paper(int r, int g, int b, int a);

/**
 * Compatibility function for setting text color (opaque)
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 */
void set_text_color(int r, int g, int b);

/**
 * Set text foreground (ink) color at specific position
 * @param x Text column
 * @param y Text row
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void poke_text_ink(int x, int y, int r, int g, int b, int a);

/**
 * Set text background (paper) color at specific position
 * @param x Text column
 * @param y Text row
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void poke_text_paper(int x, int y, int r, int g, int b, int a);

/**
 * Get text foreground (ink) color at specific position
 * @param x Text column
 * @param y Text row
 * @param r Pointer to store red component
 * @param g Pointer to store green component
 * @param b Pointer to store blue component
 * @param a Pointer to store alpha component
 */
void peek_text_ink(int x, int y, int* r, int* g, int* b, int* a);

/**
 * Get text background (paper) color at specific position
 * @param x Text column
 * @param y Text row
 * @param r Pointer to store red component
 * @param g Pointer to store green component
 * @param b Pointer to store blue component
 * @param a Pointer to store alpha component
 */
void peek_text_paper(int x, int y, int* r, int* g, int* b, int* a);

/**
 * Fill a rectangular region with both foreground and background colors
 * @param x Starting text column
 * @param y Starting text row
 * @param width Width in characters
 * @param height Height in characters
 * @param ink_r Foreground red component (0-255)
 * @param ink_g Foreground green component (0-255)
 * @param ink_b Foreground blue component (0-255)
 * @param ink_a Foreground alpha component (0-255)
 * @param paper_r Background red component (0-255)
 * @param paper_g Background green component (0-255)
 * @param paper_b Background blue component (0-255)
 * @param paper_a Background alpha component (0-255)
 */
void fill_text_color(int x, int y, int width, int height, 
                     int ink_r, int ink_g, int ink_b, int ink_a,
                     int paper_r, int paper_g, int paper_b, int paper_a);

/**
 * Fill a rectangular region with foreground color only
 * @param x Starting text column
 * @param y Starting text row
 * @param width Width in characters
 * @param height Height in characters
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void fill_text_ink(int x, int y, int width, int height, int r, int g, int b, int a);

/**
 * Fill a rectangular region with background color only
 * @param x Starting text column
 * @param y Starting text row
 * @param width Width in characters
 * @param height Height in characters
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void fill_text_paper(int x, int y, int width, int height, int r, int g, int b, int a);

/**
 * Reset all text colors to default (white on black)
 */
void clear_text_colors(void);

/**
 * Save current text buffer and color maps to backup slot n
 * @param slot Backup slot number (0-based, will expand automatically)
 */
void save_text(int slot);

/**
 * Restore text buffer and color maps from backup slot n
 * @param slot Backup slot number to restore from
 * @return true if slot exists and was restored, false otherwise
 */
bool restore_text(int slot);

/**
 * Get number of saved text backup slots
 * @return Number of available backup slots
 */
int get_saved_text_count(void);

/**
 * Clear all saved text backups to free memory
 */
void clear_saved_text(void);

/**
 * Wait for all pending render operations to complete
 * Blocks until the next frame has been fully rendered and displayed
 * Use this to measure actual render completion time, not just command queuing time
 */
void wait_for_render_complete(void);

// =============================================================================
// CURSOR SYSTEM
// =============================================================================

/**
 * Set cursor position in text grid
 * @param x Text column (0-79)
 * @param y Text row (0-24)
 */
void set_cursor_position(int x, int y);

/**
 * Set cursor visibility
 * @param visible true to show cursor, false to hide
 */
void set_cursor_visible(bool visible);

/**
 * Set cursor type
 * @param type 0=underscore, 1=block, 2=vertical bar
 */
void set_cursor_type(int type);

/**
 * Set cursor color
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255, for transparency)
 */
void set_cursor_color(int r, int g, int b, int a);

/**
 * Enable or disable cursor blinking
 * @param enable true to enable blinking, false for solid cursor
 */
void enable_cursor_blink(bool enable);

// =============================================================================
// VECTOR GRAPHICS (CAIRO-BASED)
// =============================================================================

/**
 * Clear all graphics
 */
void clear_graphics();

/**
 * Draw line between two points
 * @param x1 Start X coordinate
 * @param y1 Start Y coordinate
 * @param x2 End X coordinate
 * @param y2 End Y coordinate
 */
void draw_line(int x1, int y1, int x2, int y2);

/**
 * Draw rectangle outline
 * @param x Left coordinate
 * @param y Top coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 */
void draw_rect(int x, int y, int width, int height);

/**
 * Draw circle outline
 * @param x Center X coordinate
 * @param y Center Y coordinate  
 * @param radius Circle radius
 */
void draw_circle(int x, int y, int radius);

/**
 * Draw filled rectangle
 * @param x Left coordinate
 * @param y Top coordinate
 * @param width Rectangle width
 * @param height Rectangle height
 */
void fill_rect(int x, int y, int width, int height);

/**
 * Draw filled circle
 * @param x Center X coordinate
 * @param y Center Y coordinate
 * @param radius Circle radius
 */
void fill_circle(int x, int y, int radius);

/**
 * Set drawing color for graphics operations
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255)
 */
void set_draw_color(int r, int g, int b, int a);

// =============================================================================
// SPRITE SYSTEM
// =============================================================================

/**
 * Initialize the sprite system
 * Must be called after sprites are loaded but before using sprite instances
 * @return true on success
 */
bool init_sprites();

/**
 * Load sprite from PNG file into sprite bank
 * @param slot Sprite slot (0-511) 
 * @param filename PNG file path (relative to executable or absolute)
 * @return true on success
 */
bool load_sprite(int slot, const char* filename);

/**
 * Create or update a sprite instance
 * @param instance_id Instance ID (0-127)
 * @param sprite_slot Sprite slot from bank (0-511)
 * @param x X position in pixels
 * @param y Y position in pixels
 * @return true on success
 */
bool sprite(int instance_id, int sprite_slot, int x, int y);

/**
 * Move existing sprite instance
 * @param instance_id Instance ID (0-127)
 * @param x New X position
 * @param y New Y position
 * @return true on success
 */
bool sprite_move(int instance_id, int x, int y);

/**
 * Scale sprite instance
 * @param instance_id Instance ID (0-127)
 * @param scale_x Horizontal scale factor
 * @param scale_y Vertical scale factor
 * @return true on success
 */
bool sprite_scale(int instance_id, float scale_x, float scale_y);

/**
 * Rotate sprite instance
 * @param instance_id Instance ID (0-127)
 * @param degrees Rotation in degrees
 * @return true on success
 */
bool sprite_rotate(int instance_id, float degrees);

/**
 * Set sprite transparency
 * @param instance_id Instance ID (0-127)
 * @param alpha Alpha value (0.0 = transparent, 1.0 = opaque)
 * @return true on success
 */
bool sprite_alpha(int instance_id, float alpha);

/**
 * Set sprite Z-order for layering
 * @param instance_id Instance ID (0-127)
 * @param z_order Z order (lower = behind, higher = in front)
 * @return true on success
 */
bool sprite_z_order(int instance_id, int z_order);

/**
 * Hide sprite instance
 * @param instance_id Instance ID (0-127)
 * @return true on success
 */
bool sprite_hide(int instance_id);

/**
 * Show sprite instance
 * @param instance_id Instance ID (0-127)
 * @return true on success
 */
bool sprite_show(int instance_id);

/**
 * Check if sprite instance is visible
 * @param instance_id Instance ID (0-127)
 * @return true if visible
 */
bool sprite_is_visible(int instance_id);

/**
 * Hide all sprites
 */
void hide_all_sprites();

/**
 * Get number of active sprite instances
 * @return Number of active sprites (0-128)
 */
int get_active_sprite_count();

// =============================================================================
// TILE SYSTEM API - DUAL LAYER SUPPORT
// =============================================================================

/**
 * Initialize the tile system (both front and back layers)
 * Must be called before using any tile functions
 * @return true on success, false on failure
 */
bool init_tiles();

/**
 * Load a tile texture from file (shared by both layers)
 * @param tile_id Tile ID (1-255, 0 = empty)
 * @param filename Path to PNG file
 * @return true on success, false on failure
 */
bool load_tile(int tile_id, const char* filename);

// =============================================================================
// FRONT TILE LAYER (LAYER_TILES1) - Default/Original API
// =============================================================================

/**
 * Set tile at grid position (front layer)
 * @param grid_x Grid X coordinate
 * @param grid_y Grid Y coordinate  
 * @param tile_id Tile ID (0 = empty, 1-255 = loaded tiles)
 */
void set_tile(int grid_x, int grid_y, int tile_id);

/**
 * Get tile at grid position (front layer)
 * @param grid_x Grid X coordinate
 * @param grid_y Grid Y coordinate
 * @return Tile ID (0 = empty)
 */
int get_tile(int grid_x, int grid_y);

/**
 * Fill rectangular area with tiles (front layer)
 * @param start_x Starting grid X coordinate
 * @param start_y Starting grid Y coordinate
 * @param width Width in tiles
 * @param height Height in tiles
 * @param tile_id Tile ID to fill with
 */
void fill_tiles(int start_x, int start_y, int width, int height, int tile_id);

/**
 * Clear all tiles (front layer)
 */
void clear_tiles();

/**
 * Scroll tiles by offset (front layer)
 * @param dx X scroll delta in pixels
 * @param dy Y scroll delta in pixels
 */
void scroll_tiles(float dx, float dy);

/**
 * Set absolute tile scroll position (front layer)
 * @param x X scroll position in pixels
 * @param y Y scroll position in pixels
 */
void set_tile_scroll(float x, float y);

// =============================================================================
// BACK TILE LAYER (LAYER_TILES2) - New Parallax API
// =============================================================================

/**
 * Set tile at grid position (back layer)
 * @param grid_x Grid X coordinate
 * @param grid_y Grid Y coordinate  
 * @param tile_id Tile ID (0 = empty, 1-255 = loaded tiles)
 */
void set_back_tile(int grid_x, int grid_y, int tile_id);

/**
 * Get tile at grid position (back layer)
 * @param grid_x Grid X coordinate
 * @param grid_y Grid Y coordinate
 * @return Tile ID (0 = empty)
 */
int get_back_tile(int grid_x, int grid_y);

/**
 * Fill rectangular area with tiles (back layer)
 * @param start_x Starting grid X coordinate
 * @param start_y Starting grid Y coordinate
 * @param width Width in tiles
 * @param height Height in tiles
 * @param tile_id Tile ID to fill with
 */
void fill_back_tiles(int start_x, int start_y, int width, int height, int tile_id);

/**
 * Clear all tiles (back layer)
 */
void clear_back_tiles();

/**
 * Scroll tiles by offset (back layer)
 * @param dx X scroll delta in pixels
 * @param dy Y scroll delta in pixels
 */
void scroll_back_tiles(float dx, float dy);

/**
 * Set absolute tile scroll position (back layer)
 * @param x X scroll position in pixels
 * @param y Y scroll position in pixels
 */
void set_back_tile_scroll(float x, float y);

/**
 * Get back tile scroll position
 * @param x Pointer to store X scroll position
 * @param y Pointer to store Y scroll position
 */
void get_back_tile_scroll(float* x, float* y);

/**
 * Get front tile scroll position
 * @param x Pointer to store X scroll position
 * @param y Pointer to store Y scroll position
 */
void get_tile_scroll(float* x, float* y);

// =============================================================================
// SHARED TILE SYSTEM FUNCTIONS
// =============================================================================

/**
 * Set world map size (must be called before init_tiles)
 * @param width World map width in tiles (minimum: viewport width)
 * @param height World map height in tiles (minimum: viewport height)
 * @return true on success, false if tiles already initialized
 */
bool set_world_map_size(int width, int height);

/**
 * Get world map dimensions
 * @param width Pointer to store world map width
 * @param height Pointer to store world map height
 */
void get_world_map_size(int* width, int* height);

/**
 * Get tile grid dimensions (viewport size)
 * @param width Pointer to store grid width
 * @param height Pointer to store grid height
 */
void get_tile_grid_size(int* width, int* height);
void get_tile_scroll(float* x, float* y);

// =============================================================================
// ENHANCED INPUT SYSTEM
// =============================================================================

/**
 * Check if a key is currently pressed (immediate input)
 * Thread-safe, non-blocking
 * @param keycode Key code to check
 * @return true if key is currently pressed
 */
bool is_key_pressed(int keycode);

/**
 * Check if a key was just pressed this frame (edge detection)
 * Thread-safe, non-blocking
 * @param keycode Key code to check
 * @return true if key was pressed this frame
 */
bool key_just_pressed(int keycode);

/**
 * Check if a key was just released this frame (edge detection)
 * Thread-safe, non-blocking
 * @param keycode Key code to check
 * @return true if key was released this frame
 */
bool key_just_released(int keycode);

/**
 * Get current modifier key state
 * Thread-safe, non-blocking
 * @return Bitmask of currently pressed modifier keys
 */
uint32_t get_modifier_state();

/**
 * Get current mouse X position
 * Thread-safe, non-blocking
 * @return Mouse X coordinate in pixels
 */
int get_mouse_x();

/**
 * Get current mouse Y position
 * Thread-safe, non-blocking
 * @return Mouse Y coordinate in pixels
 */
int get_mouse_y();

/**
 * Get current mouse button state
 * Thread-safe, non-blocking
 * @return Bitmask of currently pressed mouse buttons
 */
uint32_t get_mouse_buttons();

/**
 * Get mouse wheel scroll amounts since last frame
 * Thread-safe, non-blocking
 * @param wheel_x Pointer to store horizontal scroll amount
 * @param wheel_y Pointer to store vertical scroll amount
 */
void get_mouse_wheel(int* wheel_x, int* wheel_y);

/**
 * Check if a specific mouse button is currently pressed
 * Thread-safe, non-blocking
 * @param button Button to check (MOUSE_LEFT, etc.)
 * @return true if button is currently pressed
 */
bool is_mouse_button_pressed(int button);

/**
 * Check if a mouse button was clicked this frame (edge detection)
 * Thread-safe, non-blocking
 * @param button Button to check
 * @return true if button was clicked this frame
 */
bool mouse_button_clicked(int button);

/**
 * Non-blocking character input (classic INKEY)
 * Thread-safe, non-blocking
 * @return Key code of pressed key, or 0 if no key pressed
 */
int inkey();

/**
 * Blocking character input (classic WAITKEY)
 * Thread-safe, blocking
 * @return Key code of pressed key
 */
int waitkey();

/**
 * Initialize the enhanced input system
 * Must be called after runtime initialization
 * @return true on success, false on failure
 */
bool init_input_system();

/**
 * Shutdown the enhanced input system and clean up resources
 */
void shutdown_input_system();

// =============================================================================
// INTERACTIVE REPL SYSTEM
// =============================================================================

/**
 * Get a single command from the REPL interface with split screen display
 * Top area shows output/results, bottom area provides editable input line
 * Blocks until user presses Shift+Enter or ESC
 * 
 * @param output_lines Number of lines for output area (default: 20)
 * @param prompt Prompt string to display (default: "lua> ")
 * @return Entered command string (caller must free), or NULL if cancelled/quit
 */
char* repl_get_command(int output_lines = 20, const char* prompt = "lua> ");

/**
 * Add output text to the REPL display area
 * 
 * @param text Text to add to output (supports multi-line)
 */
void repl_add_output(const char* text);

/**
 * Clear the REPL output area
 */
void repl_clear_output();

/**
 * Get FreeType face for font metrics analysis (debug/development use)
 * @return FreeType face handle or nullptr if not initialized
 */
void* get_ft_face();

/**
 * Fill a rectangular area of the color map with specified colors
 * Much more efficient than individual poke calls
 * 
 * @param start_x Starting column (0-79)
 * @param start_y Starting row (0-24)
 * @param width Width in characters
 * @param height Height in characters
 * @param ink_r Ink red component (0-255)
 * @param ink_g Ink green component (0-255)
 * @param ink_b Ink blue component (0-255)
 * @param ink_a Ink alpha component (0-255)
 * @param paper_r Paper red component (0-255)
 * @param paper_g Paper green component (0-255)
 * @param paper_b Paper blue component (0-255)
 * @param paper_a Paper alpha component (0-255)
 */
void fill_color_map(int start_x, int start_y, int width, int height,
                   int ink_r, int ink_g, int ink_b, int ink_a,
                   int paper_r, int paper_g, int paper_b, int paper_a);

/**
 * Fill a rectangular area with just paper (background) color
 * Leaves ink colors unchanged
 */
void fill_paper_map(int start_x, int start_y, int width, int height,
                   int paper_r, int paper_g, int paper_b, int paper_a);

/**
 * Fill a rectangular area with just ink (foreground) color
 * Leaves paper colors unchanged
 */
void fill_ink_map(int start_x, int start_y, int width, int height,
                 int ink_r, int ink_g, int ink_b, int ink_a);

// =============================================================================
// CONSOLE OUTPUT API
// =============================================================================

/**
 * Console output functions for application debugging and status reporting.
 * These functions allow apps to write to the terminal/console without
 * breaking the graphics context. Output appears in the terminal where
 * the runtime was started.
 */

/**
 * Print a message to the console
 * @param message The message to print
 */
void console_print(const char* message);

/**
 * Print a formatted message to the console (like printf)
 * @param format Printf-style format string
 * @param ... Additional arguments for formatting
 */
void console_printf(const char* format, ...);

/**
 * Print an info message with [INFO] prefix
 * @param message The info message to print
 */
void console_info(const char* message);

/**
 * Print a warning message with [WARNING] prefix  
 * @param message The warning message to print
 */
void console_warning(const char* message);

/**
 * Print an error message with [ERROR] prefix
 * @param message The error message to print  
 */
void console_error(const char* message);

/**
 * Print a debug message with [DEBUG] prefix (only shown in debug builds)
 * @param message The debug message to print
 */
void console_debug(const char* message);

/**
 * Start a new section in console output with a header
 * @param section_name Name of the section (e.g., "Initialization", "Main Loop")
 */
void console_section(const char* section_name);

/**
 * Print a separator line in console output
 */
void console_separator();

/**
 * Enhanced key detection that handles both ASCII and extended keys
 * @return Key code if any key is pressed, -1 if no key pressed
 * 
 * This function combines key() and is_key_pressed() to detect:
 * - ASCII keys (32-126): letters, numbers, symbols
 * - Special ASCII keys: Enter, Backspace, Escape, Delete, Tab
 * - Extended keys: Arrow keys (200-203), function keys, etc.
 */
int any_key_pressed();


#endif // ABSTRACT_RUNTIME_H