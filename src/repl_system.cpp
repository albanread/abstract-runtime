#include "abstract_runtime.h"
#include "input_system.h"
#include <SDL2/SDL.h>
#include <lua.hpp>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <sstream>

// External function declarations
bool is_terminal_input_active();

// Forward declarations
void repl_add_output(const char* text);
void repl_add_error(const char* text);

// REPL Overlay Toggle State
static std::atomic<bool> g_repl_overlay_visible{false};
static std::atomic<bool> g_repl_overlay_initialized{false};

// REPL Text Save/Load State
static std::string g_saved_repl_text;
static std::mutex g_saved_text_mutex;

// Hotkey state tracking (for edge detection)
static bool g_f8_was_pressed = false;
static bool g_f9_was_pressed = false;

// REPL limits to prevent crashes
static const int MAX_REPL_LINES = 15;       // Maximum lines in REPL input
static const int MAX_REPL_LINE_LENGTH = 100; // Maximum characters per line

// Cursor state preservation
struct CursorState {
    int x, y;
    bool visible;
    int type;
    int r, g, b, a;
    bool blink_enabled;
};
static CursorState g_saved_cursor_state;

// Screen buffer save/restore class
class ScreenBuffer {
public:
    std::vector<std::vector<uint32_t>> text_buffer;
    std::vector<std::vector<uint32_t>> ink_colors;
    std::vector<std::vector<uint32_t>> paper_colors;
    int width = 0;
    int height = 0;
    bool saved = false;
    
    void save_area(int x, int y, int w, int h);
    void restore_area(int x, int y);
};

// Global screen buffer for REPL overlay
static ScreenBuffer g_saved_screen;

// REPL Input State - similar to TerminalInput in accept_at
struct ReplInput {
    std::atomic<bool> active{false};
    std::atomic<bool> completed{false};
    std::atomic<bool> cancelled{false};
    
    std::mutex mutex;
    std::condition_variable cv;
    
    int x, y;
    int width, height;
    std::string prompt;
    std::string text;           // Single string that can contain newlines
    int cursor_pos;
    
    // Command history
    std::vector<std::string> history;
    int history_index;
    
    void clear() {
        active = false;
        completed = false;
        cancelled = false;
        text.clear();
        cursor_pos = 0;
        history_index = -1;
    }
    
    void add_to_history(const std::string& command) {
        if (command.empty()) return;
        
        if (history.empty() || history.back() != command) {
            history.push_back(command);
            if (history.size() > 100) {
                history.erase(history.begin());
            }
        }
    }
};

// =============================================================================
// LUA EXECUTION STATE
// =============================================================================

static lua_State* g_repl_lua_state = nullptr;
static std::mutex g_lua_mutex;

// Lua print function that captures output for REPL
static int lua_repl_print(lua_State* L) {
    std::ostringstream oss;
    int argc = lua_gettop(L);
    
    for (int i = 1; i <= argc; i++) {
        if (i > 1) oss << "\t";
        
        const char* str = lua_tostring(L, i);
        if (str) {
            oss << str;
        } else {
            oss << lua_typename(L, lua_type(L, i));
        }
    }
    
    // Add output to REPL
    repl_add_output(oss.str().c_str());
    return 0;
}

// Forward declarations for Lua bindings
int luaopen_abstract_runtime(lua_State* L);

// Register Lua bindings for REPL
void register_repl_lua_bindings(lua_State* L);

// Forward declaration for runtime initialization marking
void lua_mark_runtime_initialized();

// Initialize Lua state for REPL
bool init_repl_lua_state() {
    std::lock_guard<std::mutex> lock(g_lua_mutex);
    
    if (g_repl_lua_state) {
        return true; // Already initialized
    }
    
    g_repl_lua_state = luaL_newstate();
    if (!g_repl_lua_state) {
        std::cout << "[REPL] Failed to create Lua state" << std::endl;
        return false;
    }
    
    // Open standard libraries
    luaL_openlibs(g_repl_lua_state);
    
    // Override print function
    lua_pushcfunction(g_repl_lua_state, lua_repl_print);
    lua_setglobal(g_repl_lua_state, "print");
    
    // Mark runtime as pre-initialized since it's already running
    lua_mark_runtime_initialized();
    
    // Register runtime bindings
    register_repl_lua_bindings(g_repl_lua_state);
    
    std::cout << "[REPL] Lua state initialized" << std::endl;
    return true;
}

// Cleanup Lua state
void cleanup_repl_lua_state() {
    std::lock_guard<std::mutex> lock(g_lua_mutex);
    
    if (g_repl_lua_state) {
        lua_close(g_repl_lua_state);
        g_repl_lua_state = nullptr;
        std::cout << "[REPL] Lua state cleaned up" << std::endl;
    }
}

// Execute Lua code in a separate thread
void execute_lua_code_async(const std::string& code) {
    std::thread lua_thread([code]() {
        std::lock_guard<std::mutex> lock(g_lua_mutex);
        
        if (!g_repl_lua_state) {
            repl_add_error("Error: Lua state not initialized");
            return;
        }
        

        
        // Execute the Lua code
        int result = luaL_dostring(g_repl_lua_state, code.c_str());
        
        if (result != LUA_OK) {
            // Error occurred
            const char* error = lua_tostring(g_repl_lua_state, -1);
            std::string error_msg = std::string(error ? error : "Unknown error");
            repl_add_error(error_msg.c_str());
            lua_pop(g_repl_lua_state, 1);  // Remove error message
        }
    });
    
    lua_thread.detach(); // Let it run independently
}

// =============================================================================
// CURSOR NAVIGATION HELPERS
// =============================================================================

// Get current line and column from cursor position in multi-line text
void get_cursor_line_col(const std::string& text, int cursor_pos, int& line, int& col) {
    line = 0;
    col = 0;
    
    // Bounds check cursor position
    int safe_cursor_pos = std::max(0, std::min(cursor_pos, (int)text.length()));
    
    for (int i = 0; i < safe_cursor_pos && i < (int)text.length(); i++) {
        if (text[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
}

// Get cursor position from line and column in multi-line text
int get_cursor_pos_from_line_col(const std::string& text, int target_line, int target_col) {
    // Bounds check inputs
    if (target_line < 0) return 0;
    if (target_col < 0) target_col = 0;
    
    int line = 0;
    int col = 0;
    
    for (int i = 0; i <= (int)text.length(); i++) {
        if (line == target_line && col == target_col) {
            return i;
        }
        
        if (i < (int)text.length() && text[i] == '\n') {
            if (line == target_line) {
                return i; // End of target line
            }
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    
    return text.length(); // Default to end if not found
}

// Get the length of a specific line in multi-line text
int get_line_length(const std::string& text, int target_line) {
    // Bounds check target_line
    if (target_line < 0) return 0;
    
    int line = 0;
    int line_start = 0;
    
    for (int i = 0; i <= (int)text.length(); i++) {
        if (line == target_line) {
            if (i == (int)text.length() || text[i] == '\n') {
                return i - line_start;
            }
        }
        
        if (i < (int)text.length() && text[i] == '\n') {
            if (line == target_line) {
                return i - line_start;
            }
            line++;
            line_start = i + 1;
        }
    }
    
    return 0;
}

// Count total lines in multi-line text
int get_total_lines(const std::string& text) {
    int lines = 1;
    for (char c : text) {
        if (c == '\n') {
            lines++;
        }
    }
    return lines;
}

static ReplInput g_repl_input;

// Output area state
struct ReplOutputLine {
    std::string text;
    bool is_error;  // true for error (red), false for normal (white)
    
    ReplOutputLine(const std::string& t, bool err = false) : text(t), is_error(err) {}
};

struct ReplOutput {
    std::vector<ReplOutputLine> output_lines;
    int max_lines;
    
    void add_output(const std::string& text, bool is_error = false) {
        std::string line;
        for (char c : text) {
            if (c == '\n') {
                output_lines.emplace_back(line, is_error);
                line.clear();
            } else {
                line += c;
            }
        }
        if (!line.empty()) {
            output_lines.emplace_back(line, is_error);
        }
        
        // Keep only max_lines
        while ((int)output_lines.size() > max_lines) {
            output_lines.erase(output_lines.begin());
        }
    }
    
    void clear() {
        output_lines.clear();
    }
};

static ReplOutput g_repl_output;

// Update REPL display (called every frame from main thread)
void update_repl_input() {
    // Only update REPL if the overlay is visible
    if (!g_repl_overlay_visible.load() || !g_repl_input.active.load()) return;
    
    static bool debug_once = false;
    if (!debug_once) {
        std::cout << "[DEBUG REPL] update_repl_input called, REPL is active" << std::endl;
        debug_once = true;
    }
    
    std::lock_guard<std::mutex> lock(g_repl_input.mutex);
    
    // Clear entire REPL area
    for (int row = g_repl_input.y; row < g_repl_input.y + g_repl_input.height; row++) {
        std::string blank_line(g_repl_input.width, ' ');
        print_at(g_repl_input.x, row, blank_line.c_str());
    }
    
    // Display output area (top half of REPL)
    int output_start_y = g_repl_input.y;
    int output_height = g_repl_input.height / 2;
    int input_start_y = g_repl_input.y + output_height;
    
    // Show recent output lines
    int output_lines_to_show = std::min((int)g_repl_output.output_lines.size(), output_height);
    int start_line = std::max(0, (int)g_repl_output.output_lines.size() - output_lines_to_show);
    
    for (int i = 0; i < output_lines_to_show; i++) {
        const auto& output_line = g_repl_output.output_lines[start_line + i];
        if (output_line.is_error) {
            set_text_ink(255, 64, 64, 255);  // Red for errors
        } else {
            set_text_ink(200, 200, 200, 255);  // Light gray for normal output
        }
        print_at(g_repl_input.x, output_start_y + i, output_line.text.c_str());
    }
    
    // Show prompt at input area
    set_text_ink(0, 255, 0, 255);  // Green prompt
    print_at(g_repl_input.x, input_start_y, g_repl_input.prompt.c_str());
    
    // Display input text with newlines (in bottom half)
    set_text_ink(255, 255, 255, 255);  // White text
    int display_row = input_start_y;
    int col_offset = g_repl_input.prompt.length();
    int char_count = 0;
    int cursor_screen_x = g_repl_input.x + col_offset;
    int cursor_screen_y = display_row;
    int input_height = g_repl_input.height - output_height;
    
    for (size_t i = 0; i < g_repl_input.text.length(); i++) {
        char c = g_repl_input.text[i];
        
        if (char_count == g_repl_input.cursor_pos) {
            cursor_screen_x = g_repl_input.x + col_offset;
            cursor_screen_y = display_row;
        }
        
        if (c == '\n') {
            display_row++;
            col_offset = 4; // Indent continuation lines
            if (display_row >= input_start_y + input_height) break;
            
            set_text_ink(128, 255, 128, 255);  // Light green for continuation
            print_at(g_repl_input.x, display_row, "... ");
            set_text_ink(255, 255, 255, 255);  // Back to white
        } else {
            // Print single character
            char str[2] = {c, '\0'};
            print_at(g_repl_input.x + col_offset, display_row, str);
            col_offset++;
            
            if (col_offset >= g_repl_input.width) {
                display_row++;
                col_offset = 4;
                if (display_row >= input_start_y + input_height) break;
            }
        }
        char_count++;
    }
    
    // Handle cursor at end
    if (g_repl_input.cursor_pos >= (int)g_repl_input.text.length()) {
        cursor_screen_x = g_repl_input.x + col_offset;
        cursor_screen_y = display_row;
    }
    
    // Bounds check cursor position to prevent crashes (only within input area)
    cursor_screen_x = std::max(g_repl_input.x, std::min(cursor_screen_x, g_repl_input.x + g_repl_input.width - 1));
    cursor_screen_y = std::max(input_start_y, std::min(cursor_screen_y, g_repl_input.y + g_repl_input.height - 1));
    
    // No status line - keep REPL clean
    
    // Position cursor
    set_cursor_position(cursor_screen_x, cursor_screen_y);
    set_cursor_visible(true);
    set_cursor_type(1);
    set_cursor_color(255, 255, 0, 255);
}

// Screen buffer save/restore implementation
void ScreenBuffer::save_area(int x, int y, int w, int h) {
    width = w;
    height = h;
    
    text_buffer.resize(height);
    ink_colors.resize(height);
    paper_colors.resize(height);
    
    for (int row = 0; row < height; row++) {
        text_buffer[row].resize(width);
        ink_colors[row].resize(width);
        paper_colors[row].resize(width);
        
        for (int col = 0; col < width; col++) {
            uint32_t text, ink, paper;
            get_text_buffer_cell(x + col, y + row, &text, &ink, &paper);
            text_buffer[row][col] = text;
            ink_colors[row][col] = ink;
            paper_colors[row][col] = paper;
        }
    }
    
    saved = true;
    std::cout << "[REPL Overlay] Screen area saved (" << w << "x" << h << ")" << std::endl;
}

void ScreenBuffer::restore_area(int x, int y) {
    if (!saved) return;
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            set_text_buffer_cell(x + col, y + row, 
                               text_buffer[row][col],
                               ink_colors[row][col], 
                               paper_colors[row][col]);
        }
    }
    
    std::cout << "[REPL Overlay] Screen area restored (" << width << "x" << height << ")" << std::endl;
}

// Immediate F8/F9 handler functions (called from SDL event processing for instant response)
void handle_f8_pressed() {
    if (!g_repl_overlay_initialized.load()) return;
    
    // Auto-initialize input system if not already initialized
    if (!is_text_input_enabled()) {
        if (!init_input_system()) {
            // Silent failure - REPL may not work optimally
        } else {
            set_event_queuing_enabled(true);
        }
    }
    
    if (!g_repl_overlay_visible.load()) {
        g_repl_overlay_visible.store(true);
        
        if (!g_repl_input.active.load()) {
            std::lock_guard<std::mutex> lock(g_repl_input.mutex);
            g_repl_input.clear();
            
            // Save current cursor state
            // Note: In a real implementation, we'd need getter functions for cursor state
            // For now, we'll assume default values and restore them on exit
            g_saved_cursor_state.visible = false;  // Most apps don't show cursor
            g_saved_cursor_state.type = 0;         // Default underscore
            g_saved_cursor_state.r = 255;
            g_saved_cursor_state.g = 255;
            g_saved_cursor_state.b = 255;
            g_saved_cursor_state.a = 255;
            g_saved_cursor_state.blink_enabled = true;
            
            // Position overlay in center of screen
            g_repl_input.x = 10;
            g_repl_input.y = 5;
            g_repl_input.width = 60;
            g_repl_input.height = 20;
            g_repl_input.prompt = "lua> ";
            
            // Initialize output system
            g_repl_output.max_lines = g_repl_input.height / 2;
            g_repl_output.clear();
            
            // Save screen content before showing overlay
            g_saved_screen.save_area(g_repl_input.x, g_repl_input.y, 
                                   g_repl_input.width, g_repl_input.height);
            
            g_repl_input.active = true;
        }
    }
}

// F9 removed - hide REPL by typing 'quit' instead

void handle_f11_pressed() {
    if (!g_repl_overlay_initialized.load()) {
        return;
    }
    
    // F11: Execute REPL code (if REPL is visible and active)
    if (g_repl_overlay_visible.load() && g_repl_input.active.load()) {
        std::lock_guard<std::mutex> lock(g_repl_input.mutex);
        
        std::cout << "[REPL Overlay] F10 pressed - Current REPL content: '" << g_repl_input.text << "'" << std::endl;
        
        // Check if any line contains 'quit' command
        bool has_quit = false;
        std::istringstream iss(g_repl_input.text);
        std::string line;
        while (std::getline(iss, line)) {
            // Trim whitespace from line
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            if (line == "quit") {
                has_quit = true;
                break;
            }
        }
        
        if (has_quit) {
            g_repl_overlay_visible.store(false);
            g_saved_screen.restore_area(g_repl_input.x, g_repl_input.y);
            
            // Restore original cursor state
            set_cursor_visible(g_saved_cursor_state.visible);
            set_cursor_type(g_saved_cursor_state.type);
            set_cursor_color(g_saved_cursor_state.r, g_saved_cursor_state.g, 
                           g_saved_cursor_state.b, g_saved_cursor_state.a);
            enable_cursor_blink(g_saved_cursor_state.blink_enabled);
            
            g_repl_input.cancelled = true;
            g_repl_input.active = false;
            g_repl_input.cv.notify_all();
        } else {
            g_repl_input.completed = true;
            g_repl_input.cv.notify_one();

        }
    }
}

// Process REPL overlay hotkey events using direct key state checking (DEPRECATED - now handled at SDL level)
void process_repl_overlay_hotkeys() {
    // This function is now deprecated - F8/F9 are handled immediately at SDL event level
    // for instant response. Keeping this function for compatibility but it does nothing.
    return;
}

// Process REPL input events (called from main thread)
void process_repl_input_events() {
    // Only process REPL input if the overlay is visible
    if (!g_repl_overlay_visible.load() || !g_repl_input.active.load()) return;
    
    // Don't process if accept_at is active (they share the same key event queue)
    if (is_terminal_input_active()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(g_repl_input.mutex);
    
    KeyEvent event;
    while (get_next_key_event(&event)) {
        
        if (event.type != KeyEvent::KEY_DOWN) continue;
        
        // Skip F8 and F11 - let SDL-level handlers manage these for overlay control
        if (event.keycode == INPUT_KEY_F8 || event.keycode == INPUT_KEY_F11) {
            continue;
        }
        
        bool ctrl_pressed = (event.modifiers & INPUT_MOD_CTRL) != 0;
        bool shift_pressed = (event.modifiers & INPUT_MOD_SHIFT) != 0;
        
        switch (event.keycode) {
            case INPUT_KEY_ENTER:
                if (shift_pressed) {
                    // Shift+Enter: Execute
                    
                    // Check if any line contains 'quit' command
                    bool has_quit = false;
                    std::istringstream iss(g_repl_input.text);
                    std::string line;
                    while (std::getline(iss, line)) {
                        // Trim whitespace from line
                        line.erase(0, line.find_first_not_of(" \t"));
                        line.erase(line.find_last_not_of(" \t") + 1);
                        if (line == "quit") {
                            has_quit = true;
                            break;
                        }
                    }
                    
                    if (has_quit) {
                        g_repl_overlay_visible.store(false);
                        g_saved_screen.restore_area(g_repl_input.x, g_repl_input.y);
                        
                        // Restore original cursor state
                        set_cursor_visible(g_saved_cursor_state.visible);
                        set_cursor_type(g_saved_cursor_state.type);
                        set_cursor_color(g_saved_cursor_state.r, g_saved_cursor_state.g, 
                                       g_saved_cursor_state.b, g_saved_cursor_state.a);
                        enable_cursor_blink(g_saved_cursor_state.blink_enabled);
                        
                        g_repl_input.cancelled = true;
                        g_repl_input.active = false;
                        g_repl_input.cv.notify_all();
                    } else {
                        
                        // Add to history
                        if (!g_repl_input.text.empty()) {
                            g_repl_input.add_to_history(g_repl_input.text);
                        }
                        
                        // Execute Lua code in separate thread
                        execute_lua_code_async(g_repl_input.text);
                        
                        // Signal completion (original REPL behavior)
                        g_repl_input.completed = true;
                        g_repl_input.cv.notify_one();
                        

                    }
                } else {
                    // Regular Enter: Add newline (with bounds check)
                    int current_lines = get_total_lines(g_repl_input.text);
                    if (current_lines < MAX_REPL_LINES) {
                        g_repl_input.text.insert(g_repl_input.cursor_pos, 1, '\n');
                        g_repl_input.cursor_pos++;
                    }
                }
                break;
                
            case INPUT_KEY_ESCAPE:
                // Restore original cursor state before cancelling
                set_cursor_visible(g_saved_cursor_state.visible);
                set_cursor_type(g_saved_cursor_state.type);
                set_cursor_color(g_saved_cursor_state.r, g_saved_cursor_state.g, 
                               g_saved_cursor_state.b, g_saved_cursor_state.a);
                enable_cursor_blink(g_saved_cursor_state.blink_enabled);
                
                g_repl_input.cancelled = true;
                g_repl_input.completed = true;
                g_repl_input.cv.notify_one();
                break;
                
            case INPUT_KEY_BACKSPACE:
                if (g_repl_input.cursor_pos > 0 && !g_repl_input.text.empty()) {
                    g_repl_input.text.erase(g_repl_input.cursor_pos - 1, 1);
                    g_repl_input.cursor_pos--;
                }
                break;
                
            case INPUT_KEY_DELETE:
                if (g_repl_input.cursor_pos < (int)g_repl_input.text.length()) {
                    g_repl_input.text.erase(g_repl_input.cursor_pos, 1);
                }
                break;
                
            case INPUT_KEY_LEFT:
                if (g_repl_input.cursor_pos > 0) {
                    g_repl_input.cursor_pos--;
                }
                break;
                
            case INPUT_KEY_RIGHT:
                if (g_repl_input.cursor_pos < (int)g_repl_input.text.length()) {
                    g_repl_input.cursor_pos++;
                }
                break;
                
            case INPUT_KEY_UP:
                if (ctrl_pressed && !g_repl_input.history.empty()) {
                    // Ctrl+Up: History up
                    if (g_repl_input.history_index == -1) {
                        g_repl_input.history_index = g_repl_input.history.size() - 1;
                    } else if (g_repl_input.history_index > 0) {
                        g_repl_input.history_index--;
                    }
                    g_repl_input.text = g_repl_input.history[g_repl_input.history_index];
                    g_repl_input.cursor_pos = g_repl_input.text.length();
                } else if (!ctrl_pressed) {
                    // Up: Move cursor to line above (with bounds check)
                    int current_line, current_col;
                    get_cursor_line_col(g_repl_input.text, g_repl_input.cursor_pos, current_line, current_col);
                    
                    if (current_line > 0) {
                        int target_line = current_line - 1;
                        int line_length = get_line_length(g_repl_input.text, target_line);
                        int target_col = std::min(current_col, line_length);
                        int new_pos = get_cursor_pos_from_line_col(g_repl_input.text, target_line, target_col);
                        // Bounds check the new position
                        g_repl_input.cursor_pos = std::max(0, std::min(new_pos, (int)g_repl_input.text.length()));
                    }
                }
                break;
                
            case INPUT_KEY_DOWN:
                if (ctrl_pressed && g_repl_input.history_index != -1) {
                    // Ctrl+Down: History down
                    if (g_repl_input.history_index < (int)g_repl_input.history.size() - 1) {
                        g_repl_input.history_index++;
                        g_repl_input.text = g_repl_input.history[g_repl_input.history_index];
                    } else {
                        g_repl_input.history_index = -1;
                        g_repl_input.text = "";
                    }
                    g_repl_input.cursor_pos = g_repl_input.text.length();
                } else if (!ctrl_pressed) {
                    // Down: Move cursor to line below (with bounds check)
                    int current_line, current_col;
                    get_cursor_line_col(g_repl_input.text, g_repl_input.cursor_pos, current_line, current_col);
                    
                    int total_lines = get_total_lines(g_repl_input.text);
                    if (current_line < total_lines - 1) {
                        int target_line = current_line + 1;
                        int line_length = get_line_length(g_repl_input.text, target_line);
                        int target_col = std::min(current_col, line_length);
                        int new_pos = get_cursor_pos_from_line_col(g_repl_input.text, target_line, target_col);
                        // Bounds check the new position
                        g_repl_input.cursor_pos = std::max(0, std::min(new_pos, (int)g_repl_input.text.length()));
                    }
                }
                break;
                
            case INPUT_KEY_HOME:
                g_repl_input.cursor_pos = 0;
                break;
                
            case INPUT_KEY_END:
                g_repl_input.cursor_pos = g_repl_input.text.length();
                break;
                
            default:
                // Handle Ctrl combinations
                if (ctrl_pressed) {
                    if (event.keycode == 'L' || event.keycode == 'l') {
                        // Ctrl+L: Clear output
                        g_repl_output.clear();
                    } else if (event.keycode == 'U' || event.keycode == 'u') {
                        // Ctrl+U: Clear input
                        g_repl_input.text.clear();
                        g_repl_input.cursor_pos = 0;
                    } else if (event.keycode == 'K' || event.keycode == 'k') {
                        // Ctrl+K: Kill to end
                        g_repl_input.text.erase(g_repl_input.cursor_pos);
                    }
                } else if (event.keycode >= 32 && event.keycode <= 126) {
                    // Handle printable characters
                    char c = (char)event.keycode;
                    
                    // Apply shift modifier
                    if (shift_pressed) {
                        if (c >= 'a' && c <= 'z') {
                            c = c - 32;
                        } else {
                            switch (c) {
                                case '1': c = '!'; break;
                                case '2': c = '@'; break;
                                case '3': c = '#'; break;
                                case '4': c = '$'; break;
                                case '5': c = '%'; break;
                                case '6': c = '^'; break;
                                case '7': c = '&'; break;
                                case '8': c = '*'; break;
                                case '9': c = '('; break;
                                case '0': c = ')'; break;
                                case '-': c = '_'; break;
                                case '=': c = '+'; break;
                                case '[': c = '{'; break;
                                case ']': c = '}'; break;
                                case '\\': c = '|'; break;
                                case ';': c = ':'; break;
                                case '\'': c = '"'; break;
                                case ',': c = '<'; break;
                                case '.': c = '>'; break;
                                case '/': c = '?'; break;
                                case '`': c = '~'; break;
                            }
                        }
                    }
                    
                    // Check line length limit before inserting character
                    int current_line, current_col;
                    get_cursor_line_col(g_repl_input.text, g_repl_input.cursor_pos, current_line, current_col);
                    int line_length = get_line_length(g_repl_input.text, current_line);
                    
                    if (line_length < MAX_REPL_LINE_LENGTH) {
                        g_repl_input.text.insert(g_repl_input.cursor_pos, 1, c);
                        g_repl_input.cursor_pos++;
                        g_repl_input.history_index = -1; // Reset history
                    }
                
                }
                break;
        }
    }
}

// Main REPL API functions

// Initialize REPL overlay system
bool init_repl_overlay() {
    if (g_repl_overlay_initialized.load()) {
        return true; // Already initialized
    }
    
    // Initialize Lua state
    if (!init_repl_lua_state()) {
        std::cout << "[REPL] Failed to initialize Lua state" << std::endl;
        return false;
    }
    
    g_repl_overlay_visible.store(false);
    g_repl_overlay_initialized.store(true);
    g_f8_was_pressed = false;
    g_f9_was_pressed = false;
    
    std::cout << "[REPL Overlay] Initialized - Press F8 to show, F9 to hide" << std::endl;
    return true;
}

// Shutdown REPL overlay system
void shutdown_repl_overlay() {
    if (g_repl_input.active.load()) {
        std::lock_guard<std::mutex> lock(g_repl_input.mutex);
        g_repl_input.cancelled = true;
        g_repl_input.active = false;
        g_repl_input.cv.notify_all();
    }
    
    g_repl_overlay_visible.store(false);
    g_repl_overlay_initialized.store(false);
    
    // Cleanup Lua state
    cleanup_repl_lua_state();
    
    std::cout << "[REPL Overlay] Shut down" << std::endl;
}

// Check if REPL overlay is visible
bool is_repl_overlay_visible() {
    return g_repl_overlay_visible.load();
}

// Set REPL overlay visibility
void set_repl_overlay_visible(bool visible) {
    g_repl_overlay_visible.store(visible);
    
    if (visible && !g_repl_input.active.load()) {
        // Show REPL - start a new session
        std::lock_guard<std::mutex> lock(g_repl_input.mutex);
        g_repl_input.clear();
        g_repl_input.x = 10;
        g_repl_input.y = 5;
        g_repl_input.width = 60;
        g_repl_input.height = 20;
        g_repl_input.prompt = "lua> ";
        
        // Save screen content before showing overlay
        g_saved_screen.save_area(g_repl_input.x, g_repl_input.y, 
                               g_repl_input.width, g_repl_input.height);
        
        g_repl_input.active = true;
    } else if (!visible && g_repl_input.active.load()) {
        // Hide REPL - cancel session and restore screen
        std::lock_guard<std::mutex> lock(g_repl_input.mutex);
        
        // Restore screen content before hiding
        g_saved_screen.restore_area(g_repl_input.x, g_repl_input.y);
        
        g_repl_input.cancelled = true;
        g_repl_input.active = false;
        g_repl_input.cv.notify_all();
    }
}

char* repl_get_command(int output_lines, const char* prompt) {
    std::unique_lock<std::mutex> lock(g_repl_input.mutex);
    
    // Wait for any existing input to finish
    if (g_repl_input.active.load()) {
        return nullptr;
    }
    
    // Setup input session
    g_repl_input.clear();
    g_repl_input.x = 0;
    g_repl_input.y = output_lines + 1;
    g_repl_input.width = 80;
    g_repl_input.height = 25 - g_repl_input.y;
    g_repl_input.prompt = prompt ? prompt : "lua> ";
    
    // Setup output
    g_repl_output.max_lines = output_lines;
    
    // Start session
    g_repl_input.active = true;
    
    // Wait for completion
    g_repl_input.cv.wait(lock, [&] {
        return g_repl_input.completed.load() || should_quit();
    });
    
    // Get result
    char* result = nullptr;
    if (!g_repl_input.cancelled.load() && !g_repl_input.text.empty()) {
        g_repl_input.add_to_history(g_repl_input.text);
        size_t len = g_repl_input.text.length();
        result = (char*)malloc(len + 1);
        strcpy(result, g_repl_input.text.c_str());
    }
    
    // Cleanup
    g_repl_input.active = false;
    
    // Restore original cursor state instead of just hiding it
    set_cursor_visible(g_saved_cursor_state.visible);
    set_cursor_type(g_saved_cursor_state.type);
    set_cursor_color(g_saved_cursor_state.r, g_saved_cursor_state.g, 
                   g_saved_cursor_state.b, g_saved_cursor_state.a);
    enable_cursor_blink(g_saved_cursor_state.blink_enabled);
    
    return result;
}

void repl_add_output(const char* text) {
    if (!text) return;
    g_repl_output.add_output(text, false);
}

void repl_add_error(const char* text) {
    if (!text) return;
    g_repl_output.add_output(text, true);
}

void repl_clear_output() {
    g_repl_output.clear();
    
    for (int i = 0; i < g_repl_output.max_lines; i++) {
        std::string blank_line(80, ' ');
        print_at(0, i, blank_line.c_str());
    }
}

// =============================================================================
// LUA BINDINGS REGISTRATION
// =============================================================================

void register_repl_lua_bindings(lua_State* L) {
    // Register all runtime bindings
    luaopen_abstract_runtime(L);
    
    std::cout << "[REPL] Lua bindings registered" << std::endl;
}

// =============================================================================
// REPL INITIALIZATION AND CLEANUP
// =============================================================================

