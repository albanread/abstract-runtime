/**
 * @file screen_editor.cpp
 * @brief Screen Editor Implementation for Abstract Runtime
 * 
 * This implements a retro-style screen editor that works within a defined
 * rectangular area of the screen. The editor supports full text editing
 * capabilities including cursor movement, insertion, deletion, and multi-line
 * editing. It follows the same threading pattern as other input functions.
 */

#include "../include/abstract_runtime.h"
#include "../include/input_system.h"
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <iostream>

// =============================================================================
// EXTERNAL FUNCTIONS
// =============================================================================

extern bool should_quit();
extern bool get_next_key_event(KeyEvent* event);

// =============================================================================
// SCREEN EDITOR DATA STRUCTURES
// =============================================================================

struct ScreenEditorLine {
    std::string text;
    
    ScreenEditorLine() {}
    ScreenEditorLine(const std::string& t) : text(t) {}
};

struct ScreenEditor {
    // Editor area definition
    int area_x, area_y;
    int area_width, area_height;
    
    // Text content
    std::vector<ScreenEditorLine> lines;
    
    // Cursor position (relative to editor area)
    int cursor_x, cursor_y;
    
    // Scroll position (for handling content larger than area)
    int scroll_x, scroll_y;
    
    // State flags
    std::atomic<bool> active;
    std::atomic<bool> completed;
    std::atomic<bool> cancelled;
    
    // Synchronization
    std::mutex mutex;
    std::condition_variable cv;
    
    // Original cursor state (to restore after editing)
    int original_cursor_x, original_cursor_y;
    bool original_cursor_visible;
    
    ScreenEditor() : area_x(0), area_y(0), area_width(0), area_height(0),
                     cursor_x(0), cursor_y(0), scroll_x(0), scroll_y(0),
                     active(false), completed(false), cancelled(false),
                     original_cursor_x(0), original_cursor_y(0), original_cursor_visible(false) {}
    
    void clear() {
        lines.clear();
        cursor_x = cursor_y = 0;
        scroll_x = scroll_y = 0;
        completed = false;
        cancelled = false;
    }
    
    // Get the actual cursor position on screen
    int get_screen_cursor_x() const {
        return area_x + (cursor_x - scroll_x);
    }
    
    int get_screen_cursor_y() const {
        return area_y + (cursor_y - scroll_y);
    }
    
    // Check if cursor is visible in the current view
    bool is_cursor_in_view() const {
        return (cursor_x >= scroll_x && cursor_x < scroll_x + area_width &&
                cursor_y >= scroll_y && cursor_y < scroll_y + area_height);
    }
    
    // Ensure cursor is visible by adjusting scroll
    void ensure_cursor_visible() {
        // Horizontal scrolling
        if (cursor_x < scroll_x) {
            scroll_x = cursor_x;
        } else if (cursor_x >= scroll_x + area_width) {
            scroll_x = cursor_x - area_width + 1;
        }
        
        // Vertical scrolling
        if (cursor_y < scroll_y) {
            scroll_y = cursor_y;
        } else if (cursor_y >= scroll_y + area_height) {
            scroll_y = cursor_y - area_height + 1;
        }
        
        // Ensure scroll doesn't go negative
        if (scroll_x < 0) scroll_x = 0;
        if (scroll_y < 0) scroll_y = 0;
    }
    
    // Get current line, creating if necessary
    ScreenEditorLine& get_current_line() {
        while (cursor_y >= (int)lines.size()) {
            lines.emplace_back("");
        }
        return lines[cursor_y];
    }
    
    // Insert character at cursor position
    void insert_char(char ch) {
        ScreenEditorLine& line = get_current_line();
        if (cursor_x <= (int)line.text.length()) {
            line.text.insert(cursor_x, 1, ch);
            cursor_x++;
        }
    }
    
    // Insert new line at cursor position
    void insert_newline() {
        ScreenEditorLine& current_line = get_current_line();
        
        // Split the current line at cursor position
        std::string remaining_text = current_line.text.substr(cursor_x);
        current_line.text = current_line.text.substr(0, cursor_x);
        
        // Insert new line with remaining text
        cursor_y++;
        cursor_x = 0;
        lines.insert(lines.begin() + cursor_y, ScreenEditorLine(remaining_text));
    }
    
    // Delete character at cursor (backspace)
    void delete_char_back() {
        if (cursor_x > 0) {
            // Delete character before cursor
            ScreenEditorLine& line = get_current_line();
            if (cursor_x <= (int)line.text.length()) {
                line.text.erase(cursor_x - 1, 1);
                cursor_x--;
            }
        } else if (cursor_y > 0) {
            // Join with previous line
            ScreenEditorLine& current_line = get_current_line();
            cursor_y--;
            ScreenEditorLine& prev_line = get_current_line();
            cursor_x = prev_line.text.length();
            prev_line.text += current_line.text;
            lines.erase(lines.begin() + cursor_y + 1);
        }
    }
    
    // Delete character at cursor (delete key)
    void delete_char_forward() {
        ScreenEditorLine& line = get_current_line();
        if (cursor_x < (int)line.text.length()) {
            // Delete character at cursor
            line.text.erase(cursor_x, 1);
        } else if (cursor_y < (int)lines.size() - 1) {
            // Join with next line
            ScreenEditorLine& next_line = lines[cursor_y + 1];
            line.text += next_line.text;
            lines.erase(lines.begin() + cursor_y + 1);
        }
    }
    
    // Move cursor with bounds checking
    void move_cursor_left() {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            ScreenEditorLine& line = get_current_line();
            cursor_x = line.text.length();
        }
    }
    
    void move_cursor_right() {
        ScreenEditorLine& line = get_current_line();
        if (cursor_x < (int)line.text.length()) {
            cursor_x++;
        } else if (cursor_y < (int)lines.size() - 1) {
            cursor_y++;
            cursor_x = 0;
        }
    }
    
    void move_cursor_up() {
        if (cursor_y > 0) {
            cursor_y--;
            ScreenEditorLine& line = get_current_line();
            if (cursor_x > (int)line.text.length()) {
                cursor_x = line.text.length();
            }
        }
    }
    
    void move_cursor_down() {
        if (cursor_y < (int)lines.size() - 1) {
            cursor_y++;
            ScreenEditorLine& line = get_current_line();
            if (cursor_x > (int)line.text.length()) {
                cursor_x = line.text.length();
            }
        } else {
            // Create new line if at end
            lines.emplace_back("");
            cursor_y++;
            cursor_x = 0;
        }
    }
    
    // Convert editor content to string
    std::string to_string() const {
        std::string result;
        for (size_t i = 0; i < lines.size(); i++) {
            result += lines[i].text;
            if (i < lines.size() - 1) {
                result += "\n";
            }
        }
        return result;
    }
};

// Global editor state
static ScreenEditor g_screen_editor;

// =============================================================================
// SCREEN EDITOR RENDERING
// =============================================================================

static void render_editor_content() {
    // Clear the editor area first
    for (int y = 0; y < g_screen_editor.area_height; y++) {
        for (int x = 0; x < g_screen_editor.area_width; x++) {
            print_at_no_color(g_screen_editor.area_x + x, g_screen_editor.area_y + y, " ");
        }
    }
    
    // Render visible lines
    for (int view_y = 0; view_y < g_screen_editor.area_height; view_y++) {
        int line_index = g_screen_editor.scroll_y + view_y;
        if (line_index >= (int)g_screen_editor.lines.size()) {
            break;
        }
        
        const ScreenEditorLine& line = g_screen_editor.lines[line_index];
        int start_x = g_screen_editor.scroll_x;
        int chars_to_show = std::min(g_screen_editor.area_width, 
                                   (int)line.text.length() - start_x);
        
        if (chars_to_show > 0 && start_x < (int)line.text.length()) {
            std::string visible_text = line.text.substr(start_x, chars_to_show);
            print_at_no_color(g_screen_editor.area_x, g_screen_editor.area_y + view_y, 
                    visible_text.c_str());
        }
    }
    
    // Position and show cursor
    g_screen_editor.ensure_cursor_visible();
    if (g_screen_editor.is_cursor_in_view()) {
        set_cursor_position(g_screen_editor.get_screen_cursor_x(), 
                           g_screen_editor.get_screen_cursor_y());
        set_cursor_visible(true);
    }
}

// =============================================================================
// SCREEN EDITOR EVENT PROCESSING
// =============================================================================

void process_runtime_screen_editor_events() {
    if (!g_screen_editor.active.load()) {
        return;
    }
    
    KeyEvent event;
    while (get_next_key_event(&event)) {
        if (event.type != KeyEvent::KEY_DOWN) {
            continue;
        }
        
        std::lock_guard<std::mutex> lock(g_screen_editor.mutex);
        
        switch (event.keycode) {
            case INPUT_KEY_F10:
                // Complete editing
                g_screen_editor.completed = true;
                g_screen_editor.active = false;
                g_screen_editor.cv.notify_all();
                break;
                
            case INPUT_KEY_ESCAPE:
                // Cancel editing
                g_screen_editor.cancelled = true;
                g_screen_editor.active = false;
                g_screen_editor.cv.notify_all();
                break;
                
            case INPUT_KEY_LEFT:
                g_screen_editor.move_cursor_left();
                break;
                
            case INPUT_KEY_RIGHT:
                g_screen_editor.move_cursor_right();
                break;
                
            case INPUT_KEY_UP:
                g_screen_editor.move_cursor_up();
                break;
                
            case INPUT_KEY_DOWN:
                g_screen_editor.move_cursor_down();
                break;
                
            case INPUT_KEY_HOME:
                g_screen_editor.cursor_x = 0;
                break;
                
            case INPUT_KEY_END:
                {
                    ScreenEditorLine& line = g_screen_editor.get_current_line();
                    g_screen_editor.cursor_x = line.text.length();
                }
                break;
                
            case INPUT_KEY_BACKSPACE:
                g_screen_editor.delete_char_back();
                break;
                
            case INPUT_KEY_DELETE:
                g_screen_editor.delete_char_forward();
                break;
                
            case INPUT_KEY_RETURN:
                g_screen_editor.insert_newline();
                break;
                
            default:
                // Handle printable characters with shift support
                if (event.text[0] != '\0' && event.text[0] >= 32 && event.text[0] < 127) {
                    g_screen_editor.insert_char(event.text[0]);
                } else if (event.keycode >= 32 && event.keycode < 127) {
                    // Handle shift for uppercase and special characters
                    char ch = (char)event.keycode;
                    if (event.modifiers & INPUT_MOD_SHIFT) {
                        // Convert to uppercase if it's a letter
                        if (ch >= 'a' && ch <= 'z') {
                            ch = ch - 'a' + 'A';
                        } else {
                            // Handle shifted special characters
                            switch (ch) {
                                case '1': ch = '!'; break;
                                case '2': ch = '@'; break;
                                case '3': ch = '#'; break;
                                case '4': ch = '$'; break;
                                case '5': ch = '%'; break;
                                case '6': ch = '^'; break;
                                case '7': ch = '&'; break;
                                case '8': ch = '*'; break;
                                case '9': ch = '('; break;
                                case '0': ch = ')'; break;
                                case '-': ch = '_'; break;
                                case '=': ch = '+'; break;
                                case '[': ch = '{'; break;
                                case ']': ch = '}'; break;
                                case '\\': ch = '|'; break;
                                case ';': ch = ':'; break;
                                case '\'': ch = '"'; break;
                                case ',': ch = '<'; break;
                                case '.': ch = '>'; break;
                                case '/': ch = '?'; break;
                                case '`': ch = '~'; break;
                            }
                        }
                    }
                    g_screen_editor.insert_char(ch);
                }
                break;
        }
    }
}

void update_runtime_screen_editor() {
    if (!g_screen_editor.active.load()) {
        return;
    }
    
    // Update display
    render_editor_content();
}

// =============================================================================
// PUBLIC API
// =============================================================================

static void parse_initial_text(const char* initial_text) {
    g_screen_editor.lines.clear();
    
    if (!initial_text || strlen(initial_text) == 0) {
        g_screen_editor.lines.emplace_back("");
        return;
    }
    
    std::string text(initial_text);
    size_t start = 0;
    size_t pos = 0;
    
    while ((pos = text.find('\n', start)) != std::string::npos) {
        g_screen_editor.lines.emplace_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    
    // Add the last line (or the only line if no newlines)
    if (start < text.length()) {
        g_screen_editor.lines.emplace_back(text.substr(start));
    } else if (start == text.length() && text.back() == '\n') {
        // Handle case where text ends with newline
        g_screen_editor.lines.emplace_back("");
    }
    
    // Ensure we have at least one line
    if (g_screen_editor.lines.empty()) {
        g_screen_editor.lines.emplace_back("");
    }
}

char* screen_editor(int x, int y, int width, int height, const char* initial_text) {
    std::unique_lock<std::mutex> lock(g_screen_editor.mutex);
    
    // Wait for any existing editor to finish
    if (g_screen_editor.active.load()) {
        return nullptr; // Only one editor at a time
    }
    
    // Setup editor session
    g_screen_editor.clear();
    g_screen_editor.area_x = x;
    g_screen_editor.area_y = y;
    g_screen_editor.area_width = width;
    g_screen_editor.area_height = height;
    
    // Parse initial text into lines
    parse_initial_text(initial_text);
    
    // Save original cursor state
    // Note: We'll need to add functions to get current cursor state
    // For now, we'll assume cursor is at 0,0 and not visible
    g_screen_editor.original_cursor_x = 0;
    g_screen_editor.original_cursor_y = 0;
    g_screen_editor.original_cursor_visible = false;
    
    // Start the editor session
    g_screen_editor.active = true;
    
    // Wait for completion (blocks the app thread)
    g_screen_editor.cv.wait(lock, [&] {
        return g_screen_editor.completed.load() || g_screen_editor.cancelled.load() || should_quit();
    });
    
    // Get result
    char* result = nullptr;
    if (g_screen_editor.completed.load() && !g_screen_editor.cancelled.load()) {
        std::string content = g_screen_editor.to_string();
        size_t len = content.length();
        result = (char*)malloc(len + 1);
        if (result) {
            strcpy(result, content.c_str());
        }
    }
    
    // Restore original cursor state
    set_cursor_position(g_screen_editor.original_cursor_x, g_screen_editor.original_cursor_y);
    set_cursor_visible(g_screen_editor.original_cursor_visible);
    
    // Clear the editor area
    for (int cy = 0; cy < height; cy++) {
        for (int cx = 0; cx < width; cx++) {
            print_at_no_color(x + cx, y + cy, " ");
        }
    }
    
    // Clean up
    g_screen_editor.active = false;
    
    return result;
}