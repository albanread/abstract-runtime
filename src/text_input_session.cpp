#include "../include/text_input_session.h"
#include "../include/abstract_runtime.h"
#include "../include/runtime_state.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cctype>

// External functions
bool get_clipboard_text(char* buffer, int buffer_size);
bool set_clipboard_text(const char* text);

// Use external timestamp function from input_system.cpp
extern uint64_t get_current_timestamp();

// =============================================================================
// TextInputSession Implementation
// =============================================================================

TextInputSession::TextInputSession()
    : session_state_(SESSION_IDLE)
    , input_mode_(MODE_SINGLE_LINE)
    , text_("")
    , cursor_pos_(0)
    , selection_start_(0)
    , selection_end_(0)
    , display_x_(0)
    , display_y_(0)
    , display_width_(80)
    , display_height_(1)
    , scroll_offset_x_(0)
    , scroll_offset_y_(0)
    , text_color_(0xFFFFFF)
    , cursor_color_(0x00FF00)
    , selection_color_(0x0080FF)
    , background_color_(0x000000)
    , cursor_visible_(true)
    , cursor_blink_rate_(500)
    , last_cursor_blink_(0)
    , show_cursor_(true)
    , allowed_chars_("")
    , forbidden_chars_("")
    , validator_(nullptr)
    , validation_error_("")
    , echo_enabled_(true)
    , mask_char_(0)
    , max_length_(1024)
    , auto_complete_callback_(nullptr)
    , completion_index_(-1)
    , needs_render_(false)
    , text_changed_(false)
    , selection_changed_(false)
    , last_input_time_(0)
{
    // Initialize default options
    memset(&options_, 0, sizeof(options_));
    options_.max_length = max_length_;
    options_.echo_enabled = true;
    options_.text_color = text_color_;
    options_.cursor_color = cursor_color_;
    options_.background_color = background_color_;
    options_.cursor_blink_ms = cursor_blink_rate_;
}

TextInputSession::~TextInputSession() {
    end_session(false);
}

bool TextInputSession::start_session(int x, int y, int width, int height, const InputOptions* options) {
    if (session_state_ == SESSION_ACTIVE) {
        return false; // Already active
    }
    
    // Set display area
    display_x_ = x;
    display_y_ = y;
    display_width_ = width;
    display_height_ = height;
    
    // Apply options if provided
    if (options) {
        options_ = *options;
        max_length_ = options->max_length > 0 ? options->max_length : 1024;
        echo_enabled_ = options->echo_enabled;
        mask_char_ = options->mask_char;
        text_color_ = options->text_color;
        cursor_color_ = options->cursor_color;
        background_color_ = options->background_color;
        cursor_blink_rate_ = options->cursor_blink_ms > 0 ? options->cursor_blink_ms : 500;
        
        // Set input mode based on options
        if (mask_char_ != 0) {
            input_mode_ = MODE_PASSWORD;
        }
    }
    
    // Initialize state
    session_state_ = SESSION_ACTIVE;
    cursor_pos_ = 0;
    selection_start_ = 0;
    selection_end_ = 0;
    scroll_offset_x_ = 0;
    scroll_offset_y_ = 0;
    show_cursor_ = true;
    last_cursor_blink_ = ::get_current_timestamp();
    needs_render_ = true;
    
    // Clear undo/redo stacks
    undo_stack_.clear();
    redo_stack_.clear();
    push_undo_state();
    
    // Enable SDL text input
    set_text_input_enabled(true);
    set_input_focus_rect(x, y, width * 8, height * 16); // Approximate character size
    
    return true;
}

void TextInputSession::end_session(bool commit) {
    if (session_state_ != SESSION_ACTIVE) {
        return;
    }
    
    session_state_ = commit ? SESSION_COMPLETED : SESSION_CANCELLED;
    
    // Disable SDL text input
    set_text_input_enabled(false);
    
    if (!commit) {
        // Restore to initial state if cancelled
        if (!undo_stack_.empty()) {
            const UndoState& initial = undo_stack_[0];
            text_ = initial.text;
            cursor_pos_ = initial.cursor_pos;
            selection_start_ = initial.selection_start;
            selection_end_ = initial.selection_end;
        }
    }
}

void TextInputSession::cancel_session() {
    end_session(false);
}

void TextInputSession::set_text(const std::string& text) {
    if (session_state_ != SESSION_ACTIVE) return;
    
    push_undo_state();
    text_ = text;
    if (max_length_ > 0 && text_.length() > static_cast<size_t>(max_length_)) {
        text_.resize(max_length_);
    }
    cursor_pos_ = std::min(cursor_pos_, static_cast<int>(text_.length()));
    clear_selection();
    text_changed_ = true;
    trigger_render();
}

void TextInputSession::clear_text() {
    set_text("");
}

bool TextInputSession::insert_text(const std::string& text) {
    if (session_state_ != SESSION_ACTIVE || text.empty()) return false;
    
    // Filter characters
    std::string filtered_text;
    for (char c : text) {
        if (is_char_allowed(c)) {
            filtered_text += c;
        }
    }
    
    if (filtered_text.empty()) return false;
    
    push_undo_state();
    
    // Delete selection if exists
    if (has_selection()) {
        delete_selection();
    }
    
    // Check length limit
    if (max_length_ > 0) {
        int available_space = max_length_ - static_cast<int>(text_.length());
        if (available_space <= 0) return false;
        
        if (static_cast<int>(filtered_text.length()) > available_space) {
            filtered_text.resize(available_space);
        }
    }
    
    // Insert text
    text_.insert(cursor_pos_, filtered_text);
    cursor_pos_ += static_cast<int>(filtered_text.length());
    
    text_changed_ = true;
    trigger_render();
    last_input_time_ = ::get_current_timestamp();
    
    return true;
}

bool TextInputSession::insert_char(char c) {
    if (!is_char_allowed(c)) return false;
    return insert_text(std::string(1, c));
}

bool TextInputSession::delete_char() {
    if (session_state_ != SESSION_ACTIVE) return false;
    
    if (has_selection()) {
        return delete_selection();
    }
    
    if (cursor_pos_ >= static_cast<int>(text_.length())) return false;
    
    push_undo_state();
    text_.erase(cursor_pos_, 1);
    text_changed_ = true;
    trigger_render();
    
    return true;
}

bool TextInputSession::backspace() {
    if (session_state_ != SESSION_ACTIVE) return false;
    
    if (has_selection()) {
        return delete_selection();
    }
    
    if (cursor_pos_ <= 0) return false;
    
    push_undo_state();
    cursor_pos_--;
    text_.erase(cursor_pos_, 1);
    text_changed_ = true;
    trigger_render();
    
    return true;
}

bool TextInputSession::delete_word() {
    if (session_state_ != SESSION_ACTIVE) return false;
    
    int word_end = find_word_boundary(text_, cursor_pos_, true);
    if (word_end > cursor_pos_) {
        push_undo_state();
        text_.erase(cursor_pos_, word_end - cursor_pos_);
        text_changed_ = true;
        trigger_render();
        return true;
    }
    
    return false;
}

bool TextInputSession::backspace_word() {
    if (session_state_ != SESSION_ACTIVE) return false;
    
    int word_start = find_word_boundary(text_, cursor_pos_, false);
    if (word_start < cursor_pos_) {
        push_undo_state();
        text_.erase(word_start, cursor_pos_ - word_start);
        cursor_pos_ = word_start;
        text_changed_ = true;
        trigger_render();
        return true;
    }
    
    return false;
}

void TextInputSession::set_cursor_position(int position) {
    cursor_pos_ = std::max(0, std::min(position, static_cast<int>(text_.length())));
    clear_selection();
    trigger_render();
}

void TextInputSession::move_cursor(CursorMove direction, bool extend_selection) {
    if (session_state_ != SESSION_ACTIVE) return;
    
    int new_pos = cursor_pos_;
    
    switch (direction) {
        case CURSOR_LEFT:
            if (new_pos > 0) new_pos--;
            break;
            
        case CURSOR_RIGHT:
            if (new_pos < static_cast<int>(text_.length())) new_pos++;
            break;
            
        case CURSOR_HOME:
            if (input_mode_ == MODE_MULTI_LINE) {
                // Move to start of current line
                int line_start = get_line_start(get_current_line());
                new_pos = line_start;
            } else {
                new_pos = 0;
            }
            break;
            
        case CURSOR_END:
            if (input_mode_ == MODE_MULTI_LINE) {
                // Move to end of current line
                int line_end = get_line_end(get_current_line());
                new_pos = line_end;
            } else {
                new_pos = static_cast<int>(text_.length());
            }
            break;
            
        case CURSOR_UP:
            if (input_mode_ == MODE_MULTI_LINE) {
                int current_line = get_current_line();
                if (current_line > 0) {
                    int col = get_cursor_column();
                    new_pos = line_col_to_position(current_line - 1, col);
                }
            }
            break;
            
        case CURSOR_DOWN:
            if (input_mode_ == MODE_MULTI_LINE) {
                int current_line = get_current_line();
                if (current_line < get_line_count() - 1) {
                    int col = get_cursor_column();
                    new_pos = line_col_to_position(current_line + 1, col);
                }
            }
            break;
            
        case CURSOR_WORD_LEFT:
            new_pos = find_word_boundary(text_, cursor_pos_, false);
            break;
            
        case CURSOR_WORD_RIGHT:
            new_pos = find_word_boundary(text_, cursor_pos_, true);
            break;
            
        default:
            return;
    }
    
    if (extend_selection) {
        if (!has_selection()) {
            start_selection(cursor_pos_);
        }
        selection_end_ = new_pos;
        selection_changed_ = true;
    } else {
        clear_selection();
    }
    
    cursor_pos_ = new_pos;
    clamp_cursor();
    ensure_cursor_visible();
    trigger_render();
}

void TextInputSession::start_selection(int position) {
    if (position < 0) position = cursor_pos_;
    selection_start_ = position;
    selection_end_ = position;
    selection_changed_ = true;
    trigger_render();
}

void TextInputSession::end_selection() {
    selection_start_ = selection_end_ = cursor_pos_;
    selection_changed_ = true;
    trigger_render();
}

void TextInputSession::extend_selection(int position) {
    selection_end_ = std::max(0, std::min(position, static_cast<int>(text_.length())));
    selection_changed_ = true;
    trigger_render();
}

void TextInputSession::select_all() {
    selection_start_ = 0;
    selection_end_ = static_cast<int>(text_.length());
    cursor_pos_ = selection_end_;
    selection_changed_ = true;
    trigger_render();
}

void TextInputSession::clear_selection() {
    selection_start_ = selection_end_ = cursor_pos_;
    selection_changed_ = true;
    trigger_render();
}

std::string TextInputSession::get_selected_text() const {
    if (!has_selection()) return "";
    
    int start = std::min(selection_start_, selection_end_);
    int end = std::max(selection_start_, selection_end_);
    
    return text_.substr(start, end - start);
}

bool TextInputSession::delete_selection() {
    if (!has_selection()) return false;
    
    push_undo_state();
    
    int start = std::min(selection_start_, selection_end_);
    int end = std::max(selection_start_, selection_end_);
    
    text_.erase(start, end - start);
    cursor_pos_ = start;
    clear_selection();
    
    text_changed_ = true;
    trigger_render();
    
    return true;
}

bool TextInputSession::replace_selection(const std::string& text) {
    if (has_selection()) {
        delete_selection();
    }
    return insert_text(text);
}

bool TextInputSession::copy_to_clipboard() {
    if (!has_selection()) return false;
    
    std::string selected = get_selected_text();
    return set_clipboard_text(selected.c_str());
}

bool TextInputSession::cut_to_clipboard() {
    if (!copy_to_clipboard()) return false;
    return delete_selection();
}

bool TextInputSession::paste_from_clipboard() {
    char buffer[4096];
    if (!get_clipboard_text(buffer, sizeof(buffer))) return false;
    
    return insert_text(std::string(buffer));
}

void TextInputSession::push_undo_state() {
    UndoState state;
    state.text = text_;
    state.cursor_pos = cursor_pos_;
    state.selection_start = selection_start_;
    state.selection_end = selection_end_;
    state.timestamp = ::get_current_timestamp();
    
    undo_stack_.push_back(state);
    
    // Limit undo stack size
    if (undo_stack_.size() > MAX_UNDO_LEVELS) {
        undo_stack_.erase(undo_stack_.begin());
    }
    
    // Clear redo stack when new action is performed
    redo_stack_.clear();
}

bool TextInputSession::undo() {
    if (undo_stack_.size() <= 1) return false; // Keep at least the initial state
    
    // Move current state to redo stack
    UndoState current_state;
    current_state.text = text_;
    current_state.cursor_pos = cursor_pos_;
    current_state.selection_start = selection_start_;
    current_state.selection_end = selection_end_;
    current_state.timestamp = ::get_current_timestamp();
    redo_stack_.push_back(current_state);
    
    // Remove and restore previous state
    undo_stack_.pop_back();
    const UndoState& restore_state = undo_stack_.back();
    
    text_ = restore_state.text;
    cursor_pos_ = restore_state.cursor_pos;
    selection_start_ = restore_state.selection_start;
    selection_end_ = restore_state.selection_end;
    
    text_changed_ = true;
    selection_changed_ = true;
    trigger_render();
    
    return true;
}

bool TextInputSession::redo() {
    if (redo_stack_.empty()) return false;
    
    // Push current state to undo stack
    push_undo_state();
    
    // Restore from redo stack
    const UndoState& restore_state = redo_stack_.back();
    redo_stack_.pop_back();
    
    text_ = restore_state.text;
    cursor_pos_ = restore_state.cursor_pos;
    selection_start_ = restore_state.selection_start;
    selection_end_ = restore_state.selection_end;
    
    text_changed_ = true;
    selection_changed_ = true;
    trigger_render();
    
    return true;
}

void TextInputSession::clear_undo_history() {
    undo_stack_.clear();
    redo_stack_.clear();
    push_undo_state();
}

bool TextInputSession::process_key_event(const KeyEvent& event) {
    if (session_state_ != SESSION_ACTIVE || event.type != KeyEvent::KEY_DOWN) {
        return false;
    }
    
    bool handled = true;
    bool ctrl_pressed = (event.modifiers & INPUT_MOD_CTRL) != 0;
    bool shift_pressed = (event.modifiers & INPUT_MOD_SHIFT) != 0;
    bool alt_pressed = (event.modifiers & INPUT_MOD_ALT) != 0;
    
    switch (event.keycode) {
        case INPUT_KEY_ENTER:
            if (input_mode_ == MODE_MULTI_LINE) {
                insert_char('\n');
            } else {
                end_session(true);
            }
            break;
            
        case INPUT_KEY_ESCAPE:
            if (options_.allow_escape) {
                cancel_session();
            } else {
                handled = false;
            }
            break;
            
        case INPUT_KEY_TAB:
            if (options_.allow_tab) {
                if (auto_complete_callback_ && has_selection() == false) {
                    // Try auto-completion
                    std::string partial = text_.substr(0, cursor_pos_);
                    current_completions_ = auto_complete_callback_(partial);
                    if (!current_completions_.empty()) {
                        completion_index_ = 0;
                        apply_completion(0);
                    }
                } else {
                    insert_char('\t');
                }
            } else {
                end_session(true);
            }
            break;
            
        case INPUT_KEY_BACKSPACE:
            if (ctrl_pressed) {
                backspace_word();
            } else {
                backspace();
            }
            break;
            
        case INPUT_KEY_DELETE:
            if (ctrl_pressed) {
                delete_word();
            } else {
                delete_char();
            }
            break;
            
        case INPUT_KEY_LEFT:
            if (ctrl_pressed) {
                move_cursor(CURSOR_WORD_LEFT, shift_pressed);
            } else {
                move_cursor(CURSOR_LEFT, shift_pressed);
            }
            break;
            
        case INPUT_KEY_RIGHT:
            if (ctrl_pressed) {
                move_cursor(CURSOR_WORD_RIGHT, shift_pressed);
            } else {
                move_cursor(CURSOR_RIGHT, shift_pressed);
            }
            break;
            
        case INPUT_KEY_UP:
            move_cursor(CURSOR_UP, shift_pressed);
            break;
            
        case INPUT_KEY_DOWN:
            move_cursor(CURSOR_DOWN, shift_pressed);
            break;
            
        case INPUT_KEY_HOME:
            if (ctrl_pressed) {
                set_cursor_position(0);
            } else {
                move_cursor(CURSOR_HOME, shift_pressed);
            }
            break;
            
        case INPUT_KEY_END:
            if (ctrl_pressed) {
                set_cursor_position(static_cast<int>(text_.length()));
            } else {
                move_cursor(CURSOR_END, shift_pressed);
            }
            break;
            
        default:
            // Handle printable characters
            if (ctrl_pressed) {
                // Handle Ctrl+key combinations
                switch (event.keycode) {
                    case 'a': case 'A':
                        select_all();
                        break;
                    case 'c': case 'C':
                        copy_to_clipboard();
                        break;
                    case 'v': case 'V':
                        paste_from_clipboard();
                        break;
                    case 'x': case 'X':
                        cut_to_clipboard();
                        break;
                    case 'z': case 'Z':
                        if (shift_pressed) {
                            redo();
                        } else {
                            undo();
                        }
                        break;
                    case 'y': case 'Y':
                        redo();
                        break;
                    default:
                        handled = false;
                        break;
                }
            } else {
                // Handle regular character input
                if (strlen(event.text) > 0) {
                    // Use SDL's text input if available
                    insert_text(std::string(event.text));
                } else {
                    // Fall back to keycode conversion
                    char c = keycode_to_char(event.keycode, event.modifiers);
                    if (c != 0) {
                        insert_char(c);
                    } else {
                        handled = false;
                    }
                }
            }
            break;
    }
    
    return handled;
}

bool TextInputSession::process_mouse_event(const MouseEvent& event) {
    if (session_state_ != SESSION_ACTIVE) return false;
    
    // Check if mouse is within display area
    if (event.x < display_x_ || event.x >= display_x_ + display_width_ * 8 ||
        event.y < display_y_ || event.y >= display_y_ + display_height_ * 16) {
        return false;
    }
    
    if (event.type == MouseEvent::MOUSE_BUTTON_DOWN && event.button == 1) {
        // Left mouse button - position cursor
        int char_pos = pixel_to_char_x(event.x - display_x_);
        bool extend_selection = (event.modifiers & INPUT_MOD_SHIFT) != 0;
        
        if (extend_selection && !has_selection()) {
            start_selection(cursor_pos_);
        }
        
        set_cursor_position(char_pos);
        
        if (extend_selection) {
            selection_end_ = cursor_pos_;
            selection_changed_ = true;
        }
        
        return true;
    }
    
    return false;
}

void TextInputSession::update_session() {
    if (session_state_ != SESSION_ACTIVE) return;
    
    update_cursor_blink();
    ensure_cursor_visible();
    
    if (needs_render_) {
        render();
        needs_render_ = false;
    }
}

bool TextInputSession::is_char_allowed(char c) const {
    // Check printable character
    if (!is_printable_char(c) && c != '\n' && c != '\t') {
        return false;
    }
    
    // Check input mode restrictions
    switch (input_mode_) {
        case MODE_NUMERIC:
            return std::isdigit(c) || c == '.' || c == '-' || c == '+';
            
        case MODE_ALPHA_ONLY:
            return std::isalpha(c) || c == ' ';
            
        case MODE_ALPHANUMERIC:
            return std::isalnum(c) || c == ' ';
            
        default:
            break;
    }
    
    // Check allowed characters filter
    if (!allowed_chars_.empty()) {
        return allowed_chars_.find(c) != std::string::npos;
    }
    
    // Check forbidden characters filter
    if (!forbidden_chars_.empty()) {
        return forbidden_chars_.find(c) == std::string::npos;
    }
    
    return true;
}

char TextInputSession::keycode_to_char(int keycode, uint32_t modifiers) {
    bool shift_pressed = (modifiers & INPUT_MOD_SHIFT) != 0;
    
    // Handle printable ASCII characters
    if (keycode >= 32 && keycode <= 126) {
        char c = static_cast<char>(keycode);
        
        // Apply shift transformation for letters
        if (std::isalpha(c)) {
            if (shift_pressed) {
                return std::toupper(c);
            } else {
                return std::tolower(c);
            }
        }
        
        // Apply shift transformation for symbols
        if (shift_pressed) {
            switch (c) {
                case '1': return '!';
                case '2': return '@';
                case '3': return '#';
                case '4': return '$';
                case '5': return '%';
                case '6': return '^';
                case '7': return '&';
                case '8': return '*';
                case '9': return '(';
                case '0': return ')';
                case '-': return '_';
                case '=': return '+';
                case '[': return '{';
                case ']': return '}';
                case '\\': return '|';
                case ';': return ':';
                case '\'': return '"';
                case ',': return '<';
                case '.': return '>';
                case '/': return '?';
                case '`': return '~';
            }
        }
        
        return c;
    }
    
    // Handle special keys
    switch (keycode) {
        case INPUT_KEY_SPACE: return ' ';
        case INPUT_KEY_TAB: return '\t';
        default: return 0;
    }
}

bool TextInputSession::is_printable_char(char c) {
    return (c >= 32 && c <= 126) || c == '\n' || c == '\t';
}

bool TextInputSession::is_word_char(char c) {
    return std::isalnum(c) || c == '_';
}

int TextInputSession::find_word_boundary(const std::string& text, int position, bool forward) {
    if (forward) {
        // Find end of current word or start of next word
        int pos = position;
        int len = static_cast<int>(text.length());
        
        // Skip current word characters
        while (pos < len && is_word_char(text[pos])) {
            pos++;
        }
        
        // Skip non-word characters
        while (pos < len && !is_word_char(text[pos]) && text[pos] != '\n') {
            pos++;
        }
        
        return pos;
    } else {
        // Find start of current word or end of previous word
        int pos = position;
        
        // Skip back to start of current word
        while (pos > 0 && is_word_char(text[pos - 1])) {
            pos--;
        }
        
        // If we're at the start of a word, skip back through non-word characters
        if (pos == position || (pos > 0 && !is_word_char(text[pos]))) {
            while (pos > 0 && !is_word_char(text[pos - 1]) && text[pos - 1] != '\n') {
                pos--;
            }
            
            // Skip back through the previous word
            while (pos > 0 && is_word_char(text[pos - 1])) {
                pos--;
            }
        }
        
        return pos;
    }
}

void TextInputSession::normalize_selection() {
    if (selection_start_ > selection_end_) {
        std::swap(selection_start_, selection_end_);
    }
}

void TextInputSession::clamp_cursor() {
    cursor_pos_ = std::max(0, std::min(cursor_pos_, static_cast<int>(text_.length())));
}

void TextInputSession::ensure_cursor_visible() {
    // For now, just ensure cursor is within bounds
    // TODO: Implement proper scrolling for long text
    clamp_cursor();
}

void TextInputSession::update_cursor_blink() {
    uint64_t current_time = ::get_current_timestamp();
    
    if (cursor_blink_rate_ > 0 && 
        current_time - last_cursor_blink_ >= static_cast<uint64_t>(cursor_blink_rate_)) {
        show_cursor_ = !show_cursor_;
        last_cursor_blink_ = current_time;
        trigger_render();
    }
}

void TextInputSession::trigger_render() {
    needs_render_ = true;
}

void TextInputSession::render() {
    if (session_state_ != SESSION_ACTIVE) return;
    
    // Set background color and clear display area
    int bg_r = (background_color_ >> 16) & 0xFF;
    int bg_g = (background_color_ >> 8) & 0xFF;
    int bg_b = background_color_ & 0xFF;
    set_draw_color(bg_r, bg_g, bg_b, 255);
    fill_rect(display_x_, display_y_, display_width_ * 8, display_height_ * 16);
    
    // Render text content
    render_text();
    
    // Render selection if present
    if (has_selection()) {
        render_selection();
    }
    
    // Render cursor
    if (show_cursor_ && cursor_visible_) {
        render_cursor();
    }
}

void TextInputSession::render_text() {
    std::string display_text = text_;
    
    // Apply masking for password mode
    if (input_mode_ == MODE_PASSWORD && mask_char_ != 0) {
        display_text = std::string(text_.length(), mask_char_);
    }
    
    // Set text color (convert from 0xRRGGBB format)
    int r = (text_color_ >> 16) & 0xFF;
    int g = (text_color_ >> 8) & 0xFF;
    int b = text_color_ & 0xFF;
    set_text_ink(r, g, b, 255);
    
    // Render text
    print_at(display_x_ / 8, display_y_ / 16, display_text.c_str());
}

void TextInputSession::render_selection() {
    int start = std::min(selection_start_, selection_end_);
    int end = std::max(selection_start_, selection_end_);
    
    int start_x = display_x_ + start * 8;
    int width = (end - start) * 8;
    
    // Set selection color and draw background
    int sel_r = (selection_color_ >> 16) & 0xFF;
    int sel_g = (selection_color_ >> 8) & 0xFF;
    int sel_b = selection_color_ & 0xFF;
    set_draw_color(sel_r, sel_g, sel_b, 128); // Semi-transparent
    fill_rect(start_x, display_y_, width, 16);
}

void TextInputSession::render_cursor() {
    int cursor_x = display_x_ + cursor_pos_ * 8;
    
    // Set cursor color and draw vertical line
    int cur_r = (cursor_color_ >> 16) & 0xFF;
    int cur_g = (cursor_color_ >> 8) & 0xFF;
    int cur_b = cursor_color_ & 0xFF;
    set_draw_color(cur_r, cur_g, cur_b, 255);
    draw_line(cursor_x, display_y_, cursor_x, display_y_ + 15);
}

// Multi-line support methods (basic implementation)
int TextInputSession::get_line_count() const {
    if (input_mode_ != MODE_MULTI_LINE) return 1;
    return static_cast<int>(std::count(text_.begin(), text_.end(), '\n')) + 1;
}

int TextInputSession::get_current_line() const {
    if (input_mode_ != MODE_MULTI_LINE) return 0;
    return static_cast<int>(std::count(text_.begin(), text_.begin() + cursor_pos_, '\n'));
}

int TextInputSession::get_cursor_column() const {
    if (input_mode_ != MODE_MULTI_LINE) return cursor_pos_;
    
    int line_start = get_line_start(get_current_line());
    return cursor_pos_ - line_start;
}

std::vector<int> TextInputSession::get_line_breaks() const {
    std::vector<int> breaks;
    breaks.push_back(0); // Start of first line
    
    for (int i = 0; i < static_cast<int>(text_.length()); i++) {
        if (text_[i] == '\n') {
            breaks.push_back(i + 1); // Start of next line
        }
    }
    
    return breaks;
}

int TextInputSession::get_line_start(int line) const {
    std::vector<int> breaks = get_line_breaks();
    if (line < 0 || line >= static_cast<int>(breaks.size())) return 0;
    return breaks[line];
}

int TextInputSession::get_line_end(int line) const {
    std::vector<int> breaks = get_line_breaks();
    if (line < 0 || line >= static_cast<int>(breaks.size()) - 1) {
        return static_cast<int>(text_.length());
    }
    return breaks[line + 1] - 1; // Don't include the newline
}

void TextInputSession::position_to_line_col(int position, int* line, int* col) const {
    std::vector<int> breaks = get_line_breaks();
    
    *line = 0;
    for (int i = 1; i < static_cast<int>(breaks.size()); i++) {
        if (position < breaks[i]) {
            *line = i - 1;
            break;
        }
        *line = i;
    }
    
    *col = position - breaks[*line];
}

int TextInputSession::line_col_to_position(int line, int col) const {
    int line_start = get_line_start(line);
    int line_end = get_line_end(line);
    int line_length = line_end - line_start;
    
    col = std::max(0, std::min(col, line_length));
    return line_start + col;
}


int TextInputSession::pixel_to_char_x(int pixel_x) const {
    // Convert pixel position to character position
    int relative_x = pixel_x - display_x_;
    int char_pos = relative_x / 8; // Assuming 8 pixels per character
    return std::max(0, std::min(char_pos, static_cast<int>(text_.length())));
}

bool TextInputSession::apply_completion(int index) {
    if (!auto_complete_callback_ || current_completions_.empty() || 
        index < 0 || index >= static_cast<int>(current_completions_.size())) {
        return false;
    }

    // Replace current word with completion
    std::string completion = current_completions_[index];

    // Find the start of the current word
    int word_start = find_word_boundary(text_, cursor_pos_, false);

    // Replace from word start to cursor with completion
    push_undo_state();
    text_.erase(word_start, cursor_pos_ - word_start);
    text_.insert(word_start, completion);
    cursor_pos_ = word_start + static_cast<int>(completion.length());

    text_changed_ = true;
    trigger_render();

    return true;
}

void TextInputSession::set_input_mode(InputMode mode) {
    input_mode_ = mode;
    
    // Adjust display settings based on mode
    if (mode == MODE_MULTI_LINE) {
        // Enable multi-line specific features
        display_height_ = std::max(display_height_, 3 * 16); // Minimum 3 lines
    }
    
    trigger_render();
}


// =============================================================================
// C API Implementation
// =============================================================================

TextInputHandle create_text_input_session(int x, int y, int width, int height, const InputOptions* options) {
    TextInputSession* session = new TextInputSession();
    if (session->start_session(x, y, width, height, options)) {
        return static_cast<TextInputHandle>(session);
    } else {
        delete session;
        return nullptr;
    }
}

void destroy_text_input_session(TextInputHandle handle) {
    if (handle) {
        TextInputSession* session = static_cast<TextInputSession*>(handle);
        delete session;
    }
}

bool start_text_input(TextInputHandle handle) {
    if (!handle) return false;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    return session->is_active();
}

void end_text_input(TextInputHandle handle, bool commit) {
    if (!handle) return;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    session->end_session(commit);
}

char* get_text_input_result(TextInputHandle handle) {
    if (!handle) return nullptr;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    std::string text = session->get_text();
    
    char* result = static_cast<char*>(malloc(text.length() + 1));
    if (result) {
        strcpy(result, text.c_str());
    }
    return result;
}

bool update_text_input(TextInputHandle handle) {
    if (!handle) return false;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    session->update_session();
    return session->is_active();
}

bool process_text_input_key(TextInputHandle handle, const KeyEvent* event) {
    if (!handle || !event) return false;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    return session->process_key_event(*event);
}

bool process_text_input_mouse(TextInputHandle handle, const MouseEvent* event) {
    if (!handle || !event) return false;
    TextInputSession* session = static_cast<TextInputSession*>(handle);
    return session->process_mouse_event(*event);
}

// =============================================================================
// Placeholder clipboard functions (to be implemented per platform)
// =============================================================================

bool get_clipboard_text(char* buffer, int buffer_size) {
    // TODO: Implement platform-specific clipboard access
    // For now, return empty string
    if (buffer && buffer_size > 0) {
        buffer[0] = '\0';
    }
    return false;
}

bool set_clipboard_text(const char* text) {
    // TODO: Implement platform-specific clipboard access
    (void)text; // Suppress unused parameter warning
    return false;
}