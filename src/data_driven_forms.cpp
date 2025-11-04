/**
 * @file data_driven_forms.cpp
 * @brief Phase 4 Data-Driven Forms Implementation
 * 
 * This implements structured forms using embedded field markers within text content.
 * The forms use the `@:field_name:type:length` syntax and integrate with the existing
 * terminal-style input system using direct buffer access.
 */

#include "../include/input_system.h"
#include "../include/abstract_runtime.h"
#include <string>
#include <vector>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <regex>
#include <iostream>
#include <thread>
#include <chrono>

// =============================================================================
// EXTERNAL FUNCTIONS
// =============================================================================

// External input functions
extern bool get_next_key_event(KeyEvent* event);
extern bool should_quit();

// External text functions
extern void print_at(int x, int y, const char* text);
extern void set_text_ink(int r, int g, int b, int a);
extern void set_text_paper(int r, int g, int b, int a);

// =============================================================================
// FORM DATA STRUCTURES
// =============================================================================

struct DataFormField {
    std::string name;
    std::string type;
    int length;
    int display_x;
    int display_y;
    std::string value;
    int cursor_pos;
    std::string error_message;
    bool has_error;
    FieldValidator custom_validator;
    
    DataFormField() : length(0), display_x(0), display_y(0), cursor_pos(0), 
                     has_error(false), custom_validator(nullptr) {}
};

struct FormHandle {
    std::vector<DataFormField> fields;
    std::string original_text;
    std::vector<std::string> display_lines;
    int start_x;
    int start_y;
    int current_field_index;
    bool completed;
    bool cancelled;
    FormParseResult last_parse_error;
    int form_width;
    int form_height;
    
    FormHandle() : start_x(0), start_y(0), current_field_index(0), 
                   completed(false), cancelled(false), 
                   last_parse_error(FORM_PARSE_SUCCESS),
                   form_width(0), form_height(0) {}
};

// =============================================================================
// GLOBAL FORM STATE
// =============================================================================

struct FormState {
    FormHandle* active_form;
    bool processing;
    std::mutex mutex;
    std::condition_variable cv;
    
    FormState() : active_form(nullptr), processing(false) {}
};

static FormState g_form_state;

// Color constants (removed unused ones)

// =============================================================================
// FIELD VALIDATION
// =============================================================================

bool validate_string(const std::string& value) {
    (void)value; // Suppress unused parameter warning
    return true; // Strings are always valid
}

bool validate_number(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    try {
        std::stod(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool validate_integer(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isdigit(c) || c == '-' || c == '+';
    });
}

bool validate_email(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    return value.find('@') != std::string::npos && value.find('.') != std::string::npos;
}

bool validate_phone(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isdigit(c) || c == '-' || c == '(' || c == ')' || c == ' ';
    });
}

bool validate_alpha(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isalpha(c) || c == ' ';
    });
}

bool validate_alphanum(const std::string& value) {
    if (value.empty()) return true; // Allow empty
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isalnum(c) || c == ' ';
    });
}

bool validate_field_value(const std::string& type, const std::string& value) {
    if (type == "string") return validate_string(value);
    if (type == "number") return validate_number(value);
    if (type == "integer") return validate_integer(value);
    if (type == "email") return validate_email(value);
    if (type == "phone") return validate_phone(value);
    if (type == "alpha") return validate_alpha(value);
    if (type == "alphanum") return validate_alphanum(value);
    if (type == "password") return validate_string(value);
    return true; // Unknown types are treated as strings
}

// =============================================================================
// FORM PARSING
// =============================================================================

FormParseResult parse_form_text(FormHandle* form, const std::string& text) {
    form->original_text = text;
    form->fields.clear();
    form->display_lines.clear();
    
    std::istringstream iss(text);
    std::string line;
    int line_num = 0;
    
    // Regex for field markers: @:name:type:length
    std::regex field_regex(R"(@:([^:]+):([^:]+):(\d+))");
    
    while (std::getline(iss, line)) {
        std::string processed_line = line;
        std::smatch match;
        
        // Find all field markers in this line
        std::string::const_iterator search_start = line.cbegin();
        while (std::regex_search(search_start, line.cend(), match, field_regex)) {
            std::string field_name = match[1].str();
            std::string field_type = match[2].str();
            int field_length = std::stoi(match[3].str());
            
            // Validate field type
            if (field_type != "string" && field_type != "number" && 
                field_type != "integer" && field_type != "email" && 
                field_type != "phone" && field_type != "alpha" && 
                field_type != "alphanum" && field_type != "password") {
                return FORM_PARSE_INVALID_FIELD_TYPE;
            }
            
            // Create field
            DataFormField field;
            field.name = field_name;
            field.type = field_type;
            field.length = field_length;
            field.display_x = form->start_x + match.prefix().length();
            field.display_y = form->start_y + line_num;
            field.cursor_pos = 0;
            
            form->fields.push_back(field);
            
            // Replace marker with field brackets
            std::string replacement = "[" + std::string(field_length, ' ') + "]";
            size_t pos = processed_line.find(match.str());
            if (pos != std::string::npos) {
                processed_line.replace(pos, match.str().length(), replacement);
            }
            
            search_start = match.suffix().first;
        }
        
        form->display_lines.push_back(processed_line);
        line_num++;
    }
    
    if (form->fields.empty()) {
        return FORM_PARSE_NO_FIELDS;
    }
    
    form->form_height = line_num;
    form->form_width = 0;
    for (const auto& line : form->display_lines) {
        form->form_width = std::max(form->form_width, (int)line.length());
    }
    
    return FORM_PARSE_SUCCESS;
}

// =============================================================================
// DIRECT BUFFER RENDERING
// =============================================================================

void render_form_to_buffer(FormHandle* form) {
    // Render each line of the form, handling fields specially
    for (size_t i = 0; i < form->display_lines.size(); i++) {
        int y = form->start_y + i;
        if (y < 0 || y >= 25) continue;
        
        std::string line = form->display_lines[i];
        
        // Find any fields on this line
        std::vector<DataFormField*> line_fields;
        for (auto& field : form->fields) {
            if (field.display_y == y) {
                line_fields.push_back(&field);
            }
        }
        
        if (line_fields.empty()) {
            // No fields on this line - render normally
            set_text_ink(255, 255, 255, 255);  // White text
            set_text_paper(0, 0, 0, 255);      // Black background
            print_at(form->start_x, y, line.c_str());
        } else {
            // This line has fields - render text parts normally, then fields with colors
            // First render the base line normally
            set_text_ink(255, 255, 255, 255);  // White text
            set_text_paper(0, 0, 0, 255);      // Black background
            print_at(form->start_x, y, line.c_str());
            
            // Then overlay each field with colored background
            for (auto* field : line_fields) {
                // Create field display string
                std::string field_display = field->value;
                if (field->type == "password") {
                    field_display = std::string(field->value.length(), '*');
                }
                
                // Pad to field length
                if ((int)field_display.length() < field->length) {
                    field_display.append(field->length - field_display.length(), ' ');
                } else if ((int)field_display.length() > field->length) {
                    field_display = field_display.substr(0, field->length);
                }
                
                // Print field content with special colors (inside brackets)
                set_text_ink(255, 255, 0, 255);    // Yellow text
                set_text_paper(100, 150, 255, 255); // Light blue background
                print_at(field->display_x + 1, field->display_y, field_display.c_str());
            }
        }
    }
}

void update_field_cursor(FormHandle* form) {
    if (form->current_field_index < 0 || 
        form->current_field_index >= (int)form->fields.size()) return;
    
    DataFormField& field = form->fields[form->current_field_index];
    int cursor_x = field.display_x + 1 + field.cursor_pos; // +1 for opening bracket
    int cursor_y = field.display_y;
    
    // Use the new cursor system
    set_cursor_position(cursor_x, cursor_y);
    set_cursor_visible(true);
    set_cursor_type(0); // CURSOR_UNDERSCORE
    set_cursor_color(255, 255, 0, 128); // Semi-transparent yellow
    enable_cursor_blink(true);
}

// =============================================================================
// FORM EVENT PROCESSING
// =============================================================================

bool handle_form_key_event(FormHandle* form, const KeyEvent& event) {
    if (form->current_field_index < 0 || 
        form->current_field_index >= (int)form->fields.size()) {
        return false;
    }
    
    DataFormField& field = form->fields[form->current_field_index];
    
    switch (event.keycode) {
        case INPUT_KEY_ESCAPE:
            form->cancelled = true;
            form->completed = true;
            return true;
            
        case INPUT_KEY_TAB:
            if (event.modifiers & INPUT_MOD_SHIFT) {
                // Shift+Tab: Previous field
                form->current_field_index--;
                if (form->current_field_index < 0) {
                    form->current_field_index = form->fields.size() - 1;
                }
            } else {
                // Tab: Next field
                form->current_field_index++;
                if (form->current_field_index >= (int)form->fields.size()) {
                    form->current_field_index = 0;
                }
            }
            return true;
            
        case INPUT_KEY_ENTER:
            // Move to next field, or complete if last field
            form->current_field_index++;
            if (form->current_field_index >= (int)form->fields.size()) {
                form->completed = true;
            }
            return true;
            
        case INPUT_KEY_BACKSPACE:
            if (field.cursor_pos > 0) {
                field.value.erase(field.cursor_pos - 1, 1);
                field.cursor_pos--;
            }
            return true;
            
        case INPUT_KEY_DELETE:
            if (field.cursor_pos < (int)field.value.length()) {
                field.value.erase(field.cursor_pos, 1);
            }
            return true;
            
        case INPUT_KEY_LEFT:
            if (field.cursor_pos > 0) {
                field.cursor_pos--;
            }
            return true;
            
        case INPUT_KEY_RIGHT:
            if (field.cursor_pos < (int)field.value.length()) {
                field.cursor_pos++;
            }
            return true;
            
        case INPUT_KEY_HOME:
            field.cursor_pos = 0;
            return true;
            
        case INPUT_KEY_END:
            field.cursor_pos = field.value.length();
            return true;
            
        default:
            // Regular character input
            if (event.keycode >= 32 && event.keycode <= 126) {
                if ((int)field.value.length() < field.length) {
                    char ch = (char)event.keycode;
                    
                    // Handle shift modifier for uppercase and symbols
                    if (event.modifiers & INPUT_MOD_SHIFT) {
                        if (ch >= 'a' && ch <= 'z') {
                            ch = ch - 'a' + 'A'; // Convert to uppercase
                        } else {
                            // Handle shifted symbols
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
                    
                    // Type-specific filtering
                    bool allow = true;
                    if (field.type == "number" || field.type == "integer") {
                        allow = std::isdigit(ch) || ch == '.' || ch == '-' || ch == '+';
                        if (field.type == "integer" && ch == '.') allow = false;
                    } else if (field.type == "alpha") {
                        allow = std::isalpha(ch) || ch == ' ';
                    } else if (field.type == "alphanum") {
                        allow = std::isalnum(ch) || ch == ' ';
                    }
                    
                    if (allow) {
                        field.value.insert(field.cursor_pos, 1, ch);
                        field.cursor_pos++;
                    }
                }
            }
            return true;
    }
    
    return false;
}

// Called by runtime main loop to update form display
void update_runtime_form_input() {
    if (!g_form_state.processing || !g_form_state.active_form) return;
    
    FormHandle* form = g_form_state.active_form;
    render_form_to_buffer(form);
    update_field_cursor(form);
}

// Called by runtime main loop to process form input events
void process_runtime_form_input_events() {
    if (!g_form_state.processing || !g_form_state.active_form) return;
    
    FormHandle* form = g_form_state.active_form;
    KeyEvent event;
    
    while (get_next_key_event(&event)) {
        if (event.type != KeyEvent::KEY_DOWN) continue;
        
        if (handle_form_key_event(form, event)) {
            // Check if form is complete
            if (form->completed) {
                std::lock_guard<std::mutex> lock(g_form_state.mutex);
                g_form_state.processing = false;
                g_form_state.cv.notify_all();
                break;
            }
        }
    }
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

struct FormHandle* create_data_driven_form(const char* text_content, int x, int y) {
    if (!text_content) return nullptr;
    
    std::lock_guard<std::mutex> lock(g_form_state.mutex);
    
    // Only one form at a time
    if (g_form_state.active_form) return nullptr;
    
    // Create new form
    FormHandle* form = new FormHandle();
    form->start_x = x;
    form->start_y = y;
    
    // Parse the form text
    FormParseResult result = parse_form_text(form, text_content);
    if (result != FORM_PARSE_SUCCESS) {
        form->last_parse_error = result;
        delete form;
        return nullptr;
    }
    
    return form;
}

bool process_data_driven_form(struct FormHandle* form) {
    if (!form) return false;
    
    std::unique_lock<std::mutex> lock(g_form_state.mutex);
    
    // Setup form processing
    g_form_state.active_form = form;
    g_form_state.processing = true;
    form->completed = false;
    form->cancelled = false;
    form->current_field_index = 0;
    
    // Initialize cursor positions
    for (auto& field : form->fields) {
        field.cursor_pos = field.value.length();
    }
    
    // Initial render
    render_form_to_buffer(form);
    update_field_cursor(form);
    
    // Block until form completion
    g_form_state.cv.wait(lock, [&] {
        return !g_form_state.processing || should_quit();
    });
    
    // Validate fields before completing
    if (form->completed && !form->cancelled) {
        for (auto& field : form->fields) {
            if (!validate_field_value(field.type, field.value)) {
                field.has_error = true;
                field.error_message = "Invalid " + field.type + " format";
            } else {
                field.has_error = false;
                field.error_message.clear();
            }
        }
    }
    
    // Cleanup
    g_form_state.active_form = nullptr;
    g_form_state.processing = false;
    
    // Hide cursor when form is done
    set_cursor_visible(false);
    
    return form->completed && !form->cancelled;
}

const char* get_field_value(struct FormHandle* form, const char* field_name) {
    if (!form || !field_name) return nullptr;
    
    for (const auto& field : form->fields) {
        if (field.name == field_name) {
            return field.value.c_str();
        }
    }
    return nullptr;
}

bool set_field_value(struct FormHandle* form, const char* field_name, const char* value) {
    if (!form || !field_name) return false;
    
    for (auto& field : form->fields) {
        if (field.name == field_name) {
            field.value = value ? value : "";
            field.cursor_pos = field.value.length();
            return true;
        }
    }
    return false;
}

bool field_has_error(struct FormHandle* form, const char* field_name) {
    if (!form || !field_name) return false;
    
    for (const auto& field : form->fields) {
        if (field.name == field_name) {
            return field.has_error;
        }
    }
    return false;
}

const char* get_field_error(struct FormHandle* form, const char* field_name) {
    if (!form || !field_name) return nullptr;
    
    for (const auto& field : form->fields) {
        if (field.name == field_name && field.has_error) {
            return field.error_message.c_str();
        }
    }
    return nullptr;
}

bool form_has_errors(struct FormHandle* form) {
    if (!form) return false;
    
    for (const auto& field : form->fields) {
        if (field.has_error) return true;
    }
    return false;
}

int get_field_names(struct FormHandle* form, const char** names, int max_names) {
    if (!form || !names) return 0;
    
    int count = 0;
    for (const auto& field : form->fields) {
        if (count < max_names) {
            names[count] = field.name.c_str();
            count++;
        }
    }
    return count;
}

bool set_field_validator(struct FormHandle* form, const char* field_name, FieldValidator validator) {
    if (!form || !field_name) return false;
    
    for (auto& field : form->fields) {
        if (field.name == field_name) {
            field.custom_validator = validator;
            return true;
        }
    }
    return false;
}

bool format_field_value(struct FormHandle* form, const char* field_name) {
    // TODO: Implement field-specific formatting
    (void)form;
    (void)field_name;
    return true;
}

FormParseResult get_last_parse_error(struct FormHandle* form) {
    return form ? form->last_parse_error : FORM_PARSE_INVALID_HANDLE;
}

const char* get_parse_error_message(FormParseResult error) {
    switch (error) {
        case FORM_PARSE_SUCCESS: return "Success";
        case FORM_PARSE_NO_FIELDS: return "No fields found in form text";
        case FORM_PARSE_INVALID_FIELD_TYPE: return "Invalid field type specified";
        case FORM_PARSE_INVALID_FIELD_LENGTH: return "Invalid field length specified";
        case FORM_PARSE_MALFORMED_MARKER: return "Malformed field marker";
        case FORM_PARSE_INVALID_HANDLE: return "Invalid form handle";
        default: return "Unknown error";
    }
}

void destroy_data_driven_form(struct FormHandle* form) {
    if (!form) return;
    
    std::lock_guard<std::mutex> lock(g_form_state.mutex);
    
    // If this form is currently active, stop processing
    if (g_form_state.active_form == form) {
        g_form_state.active_form = nullptr;
        g_form_state.processing = false;
        g_form_state.cv.notify_all();
    }
    
    delete form;
}