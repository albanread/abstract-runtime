#include "../include/abstract_runtime.h"
#include "../include/input_system.h"
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <thread>
#include <chrono>

// Forward declaration
bool should_quit();

// =============================================================================
// TERMINAL CHARACTER INPUT STATE
// =============================================================================

struct TerminalCharInput {
    std::atomic<bool> waiting{false};
    std::queue<char> char_buffer;
    std::mutex mutex;
    std::condition_variable cv;
    
    void push_char(char c) {
        std::lock_guard<std::mutex> lock(mutex);
        char_buffer.push(c);
        cv.notify_one();
    }
    
    char pop_char_blocking() {
        std::unique_lock<std::mutex> lock(mutex);
        waiting = true;
        
        // Wait until a character is available
        cv.wait(lock, [&] {
            return !char_buffer.empty() || should_quit();
        });
        
        waiting = false;
        
        if (char_buffer.empty()) {
            return 0; // Quit was requested
        }
        
        char result = char_buffer.front();
        char_buffer.pop();
        return result;
    }
    
    char pop_char_nonblocking() {
        std::lock_guard<std::mutex> lock(mutex);
        if (char_buffer.empty()) {
            return 0; // No character available
        }
        
        char result = char_buffer.front();
        char_buffer.pop();
        return result;
    }
    
    bool has_char() {
        std::lock_guard<std::mutex> lock(mutex);
        return !char_buffer.empty();
    }
};

static TerminalCharInput g_char_input;

// =============================================================================
// TERMINAL TEXT INPUT STATE
// =============================================================================

struct TerminalInput {
    std::atomic<bool> active{false};
    std::atomic<bool> completed{false};
    std::atomic<bool> cancelled{false};
    
    std::mutex mutex;
    std::condition_variable cv;
    
    int x, y;
    int max_length;
    bool is_password;
    std::string prompt;
    std::string text;
    int cursor_pos;
    
    void clear() {
        active = false;
        completed = false;
        cancelled = false;
        text.clear();
        cursor_pos = 0;
        prompt.clear();
    }
};

static TerminalInput g_terminal_input;

// =============================================================================
// RUNTIME INTEGRATION FUNCTIONS
// =============================================================================

// These functions are called by the runtime main loop
void update_runtime_text_input() {
    if (!g_terminal_input.active.load()) return;
    
    std::lock_guard<std::mutex> lock(g_terminal_input.mutex);
    
    // Display prompt and text
    extern void set_text_ink(int r, int g, int b, int a);
    extern void print_at(int x, int y, const char* text);
    
    // Show prompt
    if (!g_terminal_input.prompt.empty()) {
        set_text_ink(255, 255, 255, 255);
        print_at(g_terminal_input.x, g_terminal_input.y, g_terminal_input.prompt.c_str());
    }
    
    // Calculate input position
    int input_x = g_terminal_input.x + (int)g_terminal_input.prompt.length();
    
    // Prepare display text
    std::string display_text = g_terminal_input.text;
    if (g_terminal_input.is_password) {
        display_text = std::string(display_text.length(), '*');
    }
    
    // Add cursor
    if (g_terminal_input.cursor_pos <= (int)display_text.length()) {
        display_text.insert(g_terminal_input.cursor_pos, "_");
    }
    
    // Clear the line and display text
    std::string clear_line(g_terminal_input.max_length + 10, ' ');
    set_text_ink(255, 255, 255, 255);
    print_at(input_x, g_terminal_input.y, clear_line.c_str());
    print_at(input_x, g_terminal_input.y, display_text.c_str());
}

void process_runtime_text_input_events() {
    if (!g_terminal_input.active.load()) return;
    
    std::lock_guard<std::mutex> lock(g_terminal_input.mutex);
    
    KeyEvent event;
    while (get_next_key_event(&event)) {
        if (event.type != KeyEvent::KEY_DOWN) continue;
        
        switch (event.keycode) {
            case INPUT_KEY_ENTER:
                g_terminal_input.completed = true;
                g_terminal_input.cv.notify_one();
                break;
                
            case INPUT_KEY_ESCAPE:
                g_terminal_input.cancelled = true;
                g_terminal_input.completed = true;
                g_terminal_input.cv.notify_one();
                break;
                
            case INPUT_KEY_BACKSPACE:
                if (g_terminal_input.cursor_pos > 0 && !g_terminal_input.text.empty()) {
                    g_terminal_input.text.erase(g_terminal_input.cursor_pos - 1, 1);
                    g_terminal_input.cursor_pos--;
                }
                break;
                
            case INPUT_KEY_DELETE:
                if (g_terminal_input.cursor_pos < (int)g_terminal_input.text.length()) {
                    g_terminal_input.text.erase(g_terminal_input.cursor_pos, 1);
                }
                break;
                
            case INPUT_KEY_LEFT:
                if (g_terminal_input.cursor_pos > 0) {
                    g_terminal_input.cursor_pos--;
                }
                break;
                
            case INPUT_KEY_RIGHT:
                if (g_terminal_input.cursor_pos < (int)g_terminal_input.text.length()) {
                    g_terminal_input.cursor_pos++;
                }
                break;
                
            case INPUT_KEY_HOME:
                g_terminal_input.cursor_pos = 0;
                break;
                
            case INPUT_KEY_END:
                g_terminal_input.cursor_pos = g_terminal_input.text.length();
                break;
                
            default:
                // Handle printable characters
                if (event.keycode >= 32 && event.keycode <= 126) {
                    if ((int)g_terminal_input.text.length() < g_terminal_input.max_length) {
                        char c = (char)event.keycode;
                        
                        // Apply shift modifier for letters
                        if (event.modifiers & INPUT_MOD_SHIFT) {
                            if (c >= 'a' && c <= 'z') {
                                c = c - 32; // Convert to uppercase
                            }
                            // Handle shifted symbols
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
                        
                        if (c) {
                            g_terminal_input.text.insert(g_terminal_input.cursor_pos, 1, c);
                            g_terminal_input.cursor_pos++;
                        }
                    }
                }
                
                // Also feed character to RDCH system (only for text input mode)
                if (event.keycode >= 32 && event.keycode <= 126) {
                    char c = (char)event.keycode;
                    // Apply shift modifier
                    if (event.modifiers & INPUT_MOD_SHIFT) {
                        if (c >= 'a' && c <= 'z') {
                            c = c - 32;
                        }
                    }
                    g_char_input.push_char(c);
                } else {
                    // Feed special keys to RDCH as well
                    switch (event.keycode) {
                        case INPUT_KEY_ENTER:
                            g_char_input.push_char('\r');
                            break;
                        case INPUT_KEY_BACKSPACE:
                            g_char_input.push_char('\b');
                            break;
                        case INPUT_KEY_ESCAPE:
                            g_char_input.push_char(27);
                            break;
                        case INPUT_KEY_DELETE:
                            g_char_input.push_char(127);
                            break;
                    }
                }
                break;
        }
    }
}

// =============================================================================
// CHARACTER INPUT PROCESSING
// =============================================================================

// Process character input events when NOT in text input mode
void process_char_input_events() {
    if (g_terminal_input.active.load()) {
        return; // Text input mode is active, character events handled there
    }
    
    KeyEvent event;
    while (get_next_key_event(&event)) {
        if (event.type != KeyEvent::KEY_DOWN) continue;
        
        // Handle printable characters
        if (event.keycode >= 32 && event.keycode <= 126) {
            char c = (char)event.keycode;
            
            // Apply shift modifier
            if (event.modifiers & INPUT_MOD_SHIFT) {
                if (c >= 'a' && c <= 'z') {
                    c = c - 32; // Convert to uppercase
                }
                // Handle shifted symbols
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
            
            g_char_input.push_char(c);
        } else {
            // Handle special keys
            switch (event.keycode) {
                case INPUT_KEY_ENTER:
                    g_char_input.push_char('\r');
                    break;
                case INPUT_KEY_BACKSPACE:
                    g_char_input.push_char('\b');
                    break;
                case INPUT_KEY_ESCAPE:
                    g_char_input.push_char(27);
                    break;
                case INPUT_KEY_DELETE:
                    g_char_input.push_char(127);
                    break;
            }
        }
    }
}

// =============================================================================
// PUBLIC API FUNCTIONS
// =============================================================================

char* accept_at(int x, int y, int max_length) {
    return accept_at_with_prompt(x, y, "", max_length);
}

char* accept_at_with_prompt(int x, int y, const char* prompt, int max_length) {
    std::unique_lock<std::mutex> lock(g_terminal_input.mutex);
    
    // Wait for any existing input to finish
    if (g_terminal_input.active.load()) {
        return nullptr; // Only one input at a time
    }
    
    // Setup input session
    g_terminal_input.clear();
    g_terminal_input.x = x;
    g_terminal_input.y = y;
    g_terminal_input.max_length = max_length > 0 ? max_length : 50;
    g_terminal_input.is_password = false;
    g_terminal_input.prompt = prompt ? prompt : "";
    g_terminal_input.cursor_pos = 0;
    
    // Start the session
    g_terminal_input.active = true;
    
    // Wait for completion (blocks the app thread like a true terminal)
    extern bool should_quit();
    g_terminal_input.cv.wait(lock, [&] {
        return g_terminal_input.completed.load() || should_quit();
    });
    
    // Get result
    char* result = nullptr;
    if (!g_terminal_input.cancelled.load() && !g_terminal_input.text.empty()) {
        size_t len = g_terminal_input.text.length();
        result = (char*)malloc(len + 1);
        strcpy(result, g_terminal_input.text.c_str());
    }
    
    // Clean up
    g_terminal_input.active = false;
    
    return result;
}

// Function to check if terminal input is active (for REPL coordination)
bool is_terminal_input_active() {
    return g_terminal_input.active.load();
}

// =============================================================================
// CHARACTER INPUT API - THE RDCH MAGIC
// =============================================================================

char rdch() {
    return g_char_input.pop_char_blocking();
}

char rdch_nowait() {
    return g_char_input.pop_char_nonblocking();
}

bool char_available() {
    return g_char_input.has_char();
}

int key() {
    // Check immediate key state - returns character if key currently pressed, -1 if not
    // This is different from rdch - it checks physical key state, not buffered input
    // 
    // NOTE: This function only returns ASCII character codes (32-126) plus a few
    // special keys with ASCII equivalents. It does NOT return arrow keys (200-203)
    // or other extended keys. Use is_key_pressed() for arrow keys and function keys.
    
    // Check printable characters (space through ~)
    for (int keycode = 32; keycode <= 126; keycode++) {
        if (is_key_pressed(keycode)) {
            char c = (char)keycode;
            
            // Apply shift modifier if shift is currently held
            uint32_t modifiers = get_modifier_state();
            if (modifiers & (INPUT_MOD_LSHIFT | INPUT_MOD_RSHIFT)) {
                if (c >= 'a' && c <= 'z') {
                    c = c - 32; // Convert to uppercase
                }
                // Handle shifted symbols
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
            return (int)c;
        }
    }
    
    // Check special keys that have character equivalents
    if (is_key_pressed(INPUT_KEY_ENTER)) return '\r';
    if (is_key_pressed(INPUT_KEY_BACKSPACE)) return '\b';
    if (is_key_pressed(INPUT_KEY_ESCAPE)) return 27;
    if (is_key_pressed(INPUT_KEY_DELETE)) return 127;
    if (is_key_pressed(INPUT_KEY_TAB)) return '\t';
    
    // Arrow keys and other extended keys (200+) are intentionally NOT handled here
    // because they don't have ASCII character equivalents. Use is_key_pressed() instead:
    // - is_key_pressed(INPUT_KEY_LEFT) for left arrow
    // - is_key_pressed(INPUT_KEY_RIGHT) for right arrow  
    // - is_key_pressed(INPUT_KEY_UP) for up arrow
    // - is_key_pressed(INPUT_KEY_DOWN) for down arrow
    
    // No key currently pressed
    return -1;
}

char* accept_password_at(int x, int y, int max_length) {
    std::unique_lock<std::mutex> lock(g_terminal_input.mutex);
    
    // Wait for any existing input to finish
    if (g_terminal_input.active.load()) {
        return nullptr; // Only one input at a time
    }
    
    // Setup input session
    g_terminal_input.clear();
    g_terminal_input.x = x;
    g_terminal_input.y = y;
    g_terminal_input.max_length = max_length > 0 ? max_length : 50;
    g_terminal_input.is_password = true;
    g_terminal_input.prompt = "";
    g_terminal_input.cursor_pos = 0;
    
    // Start the session
    g_terminal_input.active = true;
    
    // Wait for completion (blocks the app thread like a true terminal)
    extern bool should_quit();
    g_terminal_input.cv.wait(lock, [&] {
        return g_terminal_input.completed.load() || should_quit();
    });
    
    // Get result
    char* result = nullptr;
    if (!g_terminal_input.cancelled.load() && !g_terminal_input.text.empty()) {
        size_t len = g_terminal_input.text.length();
        result = (char*)malloc(len + 1);
        strcpy(result, g_terminal_input.text.c_str());
    }
    
    // Clean up
    g_terminal_input.active = false;
    
    return result;
}