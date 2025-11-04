#ifndef TEXT_INPUT_SESSION_H
#define TEXT_INPUT_SESSION_H

#include <string>
#include <cstdint>
#include <vector>
#include "input_system.h"



// =============================================================================
// TEXT INPUT SESSION
// =============================================================================

/**
 * Text input session management class for advanced text input handling
 * Supports cursor management, selection, clipboard operations, undo/redo, etc.
 */
class TextInputSession {
public:
    // Session state flags
    enum SessionState {
        SESSION_IDLE = 0,
        SESSION_ACTIVE,
        SESSION_COMPLETED,
        SESSION_CANCELLED
    };

    // Text input mode
    enum InputMode {
        MODE_SINGLE_LINE = 0,
        MODE_MULTI_LINE,
        MODE_PASSWORD,
        MODE_NUMERIC,
        MODE_ALPHA_ONLY,
        MODE_ALPHANUMERIC
    };

    // Cursor movement direction
    enum CursorMove {
        CURSOR_LEFT = 0,
        CURSOR_RIGHT,
        CURSOR_UP,
        CURSOR_DOWN,
        CURSOR_HOME,
        CURSOR_END,
        CURSOR_PAGE_UP,
        CURSOR_PAGE_DOWN,
        CURSOR_WORD_LEFT,
        CURSOR_WORD_RIGHT
    };

    // Constructor/Destructor
    TextInputSession();
    ~TextInputSession();

    // Session management
    bool start_session(int x, int y, int width, int height, const InputOptions* options = nullptr);
    void end_session(bool commit = true);
    void cancel_session();
    SessionState get_session_state() const { return session_state_; }
    bool is_active() const { return session_state_ == SESSION_ACTIVE; }

    // Text content management
    void set_text(const std::string& text);
    std::string get_text() const { return text_; }
    void clear_text();
    
    // Text editing operations
    bool insert_text(const std::string& text);
    bool insert_char(char c);
    bool delete_char();           // Delete at cursor position
    bool backspace();            // Delete before cursor position
    bool delete_word();          // Delete word at cursor
    bool backspace_word();       // Delete word before cursor
    
    // Cursor management
    void set_cursor_position(int position);
    int get_cursor_position() const { return cursor_pos_; }
    void move_cursor(CursorMove direction, bool extend_selection = false);
    void move_cursor_to_xy(int x, int y, bool extend_selection = false);
    
    // Selection management
    void start_selection(int position = -1);  // -1 = current cursor position
    void end_selection();
    void extend_selection(int position);
    void select_all();
    void clear_selection();
    bool has_selection() const { return selection_start_ != selection_end_; }
    int get_selection_start() const { return selection_start_; }
    int get_selection_end() const { return selection_end_; }
    std::string get_selected_text() const;
    bool delete_selection();
    bool replace_selection(const std::string& text);
    
    // Clipboard operations
    bool copy_to_clipboard();
    bool cut_to_clipboard();
    bool paste_from_clipboard();
    
    // Undo/Redo operations
    void push_undo_state();
    bool undo();
    bool redo();
    void clear_undo_history();
    
    // Event processing
    bool process_key_event(const KeyEvent& event);
    bool process_mouse_event(const MouseEvent& event);
    void update_session();  // Called each frame to handle cursor blink, etc.
    
    // Validation and filtering
    void set_input_filter(const std::string& allowed_chars);
    void set_forbidden_chars(const std::string& forbidden_chars);
    bool set_validator(bool (*validator)(const char* text));
    bool is_valid() const;
    std::string get_validation_error() const { return validation_error_; }
    
    // Display properties
    void set_display_area(int x, int y, int width, int height);
    void get_display_area(int* x, int* y, int* width, int* height) const;
    void set_colors(uint32_t text_color, uint32_t cursor_color, uint32_t selection_color);
    void set_cursor_blink_rate(int milliseconds);
    
    // Auto-completion support
    void set_auto_complete_callback(std::vector<std::string> (*callback)(const std::string& partial));
    std::vector<std::string> get_completions() const;
    bool apply_completion(int index);
    
    // Multi-line support
    void set_input_mode(InputMode mode);
    InputMode get_input_mode() const { return input_mode_; }
    int get_line_count() const;
    int get_current_line() const;
    int get_cursor_column() const;
    void move_to_line(int line);
    std::string get_line_text(int line) const;
    
    // Character conversion utilities
    static char keycode_to_char(int keycode, uint32_t modifiers);
    static bool is_printable_char(char c);
    static bool is_word_char(char c);
    static int find_word_boundary(const std::string& text, int position, bool forward = true);

    // Rendering support
    void render();  // Renders the text input session to screen
    bool needs_render() const { return needs_render_; }

private:
    // Internal state
    SessionState session_state_;
    InputMode input_mode_;
    
    // Text content
    std::string text_;
    int cursor_pos_;
    int selection_start_;
    int selection_end_;
    
    // Display area
    int display_x_, display_y_;
    int display_width_, display_height_;
    int scroll_offset_x_, scroll_offset_y_;
    
    // Visual properties
    uint32_t text_color_;
    uint32_t cursor_color_;
    uint32_t selection_color_;
    uint32_t background_color_;
    
    // Cursor management
    bool cursor_visible_;
    int cursor_blink_rate_;
    uint64_t last_cursor_blink_;
    bool show_cursor_;
    
    // Input filtering and validation
    std::string allowed_chars_;
    std::string forbidden_chars_;
    bool (*validator_)(const char* text);
    std::string validation_error_;
    
    // Input options
    InputOptions options_;
    bool echo_enabled_;
    char mask_char_;
    int max_length_;
    
    // Auto-completion
    std::vector<std::string> (*auto_complete_callback_)(const std::string& partial);
    std::vector<std::string> current_completions_;
    int completion_index_;
    
    // Undo/Redo system
    struct UndoState {
        std::string text;
        int cursor_pos;
        int selection_start;
        int selection_end;
        uint64_t timestamp;
    };
    std::vector<UndoState> undo_stack_;
    std::vector<UndoState> redo_stack_;
    static const int MAX_UNDO_LEVELS = 100;
    
    // Internal flags
    bool needs_render_;
    bool text_changed_;
    bool selection_changed_;
    uint64_t last_input_time_;
    
    // Helper methods
    void normalize_selection();
    void clamp_cursor();
    void ensure_cursor_visible();
    void update_cursor_blink();
    bool is_char_allowed(char c) const;
    int char_to_pixel_x(int char_pos) const;
    int pixel_to_char_x(int pixel_x) const;
    void render_text();
    void render_cursor();
    void render_selection();
    void trigger_render();
    
    // Multi-line helpers
    std::vector<int> get_line_breaks() const;
    int get_line_start(int line) const;
    int get_line_end(int line) const;
    void position_to_line_col(int position, int* line, int* col) const;
    int line_col_to_position(int line, int col) const;
};

// =============================================================================
// C API WRAPPER FUNCTIONS
// =============================================================================

/**
 * C API wrapper for TextInputSession
 * These functions provide a simple C interface to the TextInputSession class
 */

// Session handle (opaque pointer)
typedef void* TextInputHandle;

/**
 * Create a new text input session
 * @param x Starting X position
 * @param y Starting Y position  
 * @param width Width of input area
 * @param height Height of input area
 * @param options Input options (can be NULL for defaults)
 * @return Session handle, or NULL on failure
 */
TextInputHandle create_text_input_session(int x, int y, int width, int height, const InputOptions* options);

/**
 * Destroy a text input session
 * @param handle Session handle
 */
void destroy_text_input_session(TextInputHandle handle);

/**
 * Start the text input session
 * @param handle Session handle
 * @return true on success, false on failure
 */
bool start_text_input(TextInputHandle handle);

/**
 * End the text input session
 * @param handle Session handle
 * @param commit Whether to commit the changes (true) or cancel (false)
 */
void end_text_input(TextInputHandle handle, bool commit);

/**
 * Get the current text from the session
 * @param handle Session handle
 * @return Current text (caller must free the returned string)
 */
char* get_text_input_result(TextInputHandle handle);

/**
 * Update the text input session (call each frame)
 * @param handle Session handle
 * @return true if session is still active, false if completed/cancelled
 */
bool update_text_input(TextInputHandle handle);

/**
 * Process a key event in the text input session
 * @param handle Session handle
 * @param event Pointer to key event
 * @return true if event was handled, false otherwise
 */
bool process_text_input_key(TextInputHandle handle, const KeyEvent* event);

/**
 * Process a mouse event in the text input session
 * @param handle Session handle
 * @param event Pointer to mouse event
 * @return true if event was handled, false otherwise
 */
bool process_text_input_mouse(TextInputHandle handle, const MouseEvent* event);



#endif // TEXT_INPUT_SESSION_H