#ifndef INPUT_SYSTEM_H
#define INPUT_SYSTEM_H

#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include "runtime_state.h"



// =============================================================================
// INPUT CONSTANTS
// =============================================================================

// Maximum limits (core constants defined in runtime_state.h)
constexpr int MAX_INPUT_LINE_LENGTH = 1024;

// Mouse button masks (can be combined)
constexpr uint32_t INPUT_MOUSE_LEFT = 0x01;
constexpr uint32_t INPUT_MOUSE_RIGHT = 0x02;
constexpr uint32_t INPUT_MOUSE_MIDDLE = 0x04;
constexpr uint32_t INPUT_MOUSE_X1 = 0x08;
constexpr uint32_t INPUT_MOUSE_X2 = 0x10;

// Key codes (extended from abstract_runtime.h)
constexpr int INPUT_KEY_UNKNOWN = 0;
constexpr int INPUT_KEY_SPACE = 32;
constexpr int INPUT_KEY_ENTER = 13;
constexpr int INPUT_KEY_RETURN = 13;
constexpr int INPUT_KEY_TAB = 9;
constexpr int INPUT_KEY_ESCAPE = 27;
constexpr int INPUT_KEY_BACKSPACE = 8;
constexpr int INPUT_KEY_DELETE = 127;

// Arrow keys
constexpr int INPUT_KEY_LEFT = 200;
constexpr int INPUT_KEY_RIGHT = 201;
constexpr int INPUT_KEY_UP = 202;
constexpr int INPUT_KEY_DOWN = 203;

// Navigation keys
constexpr int INPUT_KEY_HOME = 204;
constexpr int INPUT_KEY_END = 205;
constexpr int INPUT_KEY_PAGE_UP = 206;
constexpr int INPUT_KEY_PAGE_DOWN = 207;
constexpr int INPUT_KEY_INSERT = 208;

// Function keys
constexpr int INPUT_KEY_F1 = 210;
constexpr int INPUT_KEY_F2 = 211;
constexpr int INPUT_KEY_F3 = 212;
constexpr int INPUT_KEY_F4 = 213;
constexpr int INPUT_KEY_F5 = 214;
constexpr int INPUT_KEY_F6 = 215;
constexpr int INPUT_KEY_F7 = 216;
constexpr int INPUT_KEY_F8 = 217;
constexpr int INPUT_KEY_F9 = 218;
constexpr int INPUT_KEY_F10 = 219;
constexpr int INPUT_KEY_F11 = 220;
constexpr int INPUT_KEY_F12 = 221;

// Modifier keys
constexpr int INPUT_KEY_LSHIFT = 230;
constexpr int INPUT_KEY_RSHIFT = 231;
constexpr int INPUT_KEY_LCTRL = 232;
constexpr int INPUT_KEY_RCTRL = 233;
constexpr int INPUT_KEY_LALT = 234;
constexpr int INPUT_KEY_RALT = 235;
constexpr int INPUT_KEY_LGUI = 236;
constexpr int INPUT_KEY_RGUI = 237;

// Modifier state flags
constexpr uint32_t INPUT_MOD_NONE = 0x0000;
constexpr uint32_t INPUT_MOD_LSHIFT = 0x0001;
constexpr uint32_t INPUT_MOD_RSHIFT = 0x0002;
constexpr uint32_t INPUT_MOD_LCTRL = 0x0040;
constexpr uint32_t INPUT_MOD_RCTRL = 0x0080;
constexpr uint32_t INPUT_MOD_LALT = 0x0100;
constexpr uint32_t INPUT_MOD_RALT = 0x0200;
constexpr uint32_t INPUT_MOD_LGUI = 0x0400;
constexpr uint32_t INPUT_MOD_RGUI = 0x0800;
constexpr uint32_t INPUT_MOD_SHIFT = (INPUT_MOD_LSHIFT | INPUT_MOD_RSHIFT);
constexpr uint32_t INPUT_MOD_CTRL = (INPUT_MOD_LCTRL | INPUT_MOD_RCTRL);
constexpr uint32_t INPUT_MOD_ALT = (INPUT_MOD_LALT | INPUT_MOD_RALT);
constexpr uint32_t INPUT_MOD_GUI = (INPUT_MOD_LGUI | INPUT_MOD_RGUI);

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * Mouse event structure
 */
struct MouseEvent {
    enum Type {
        MOUSE_NONE = 0,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_MOTION,
        MOUSE_WHEEL
    } type;
    
    int x, y;                    // Mouse position
    int button;                  // Button number (1=left, 2=middle, 3=right)
    int wheel_x, wheel_y;        // Wheel scroll amounts
    uint32_t modifiers;          // Modifier key state
    uint64_t timestamp;          // Event timestamp
};

/**
 * Keyboard event structure
 */
struct KeyEvent {
    enum Type {
        KEY_NONE = 0,
        KEY_DOWN,
        KEY_UP
    } type;
    
    int keycode;                 // Key code (INPUT_KEY_* constants)
    uint32_t modifiers;          // Modifier key state
    char text[32];               // UTF-8 text representation (for text input)
    uint64_t timestamp;          // Event timestamp
};

/**
 * Input options for text input functions
 */
struct InputOptions {
    int max_length;              // Maximum input length (0 = no limit)
    bool echo_enabled;           // Whether to display typed characters
    char mask_char;              // Character to display instead of actual input (0 = no mask)
    uint32_t text_color;         // Color for input text
    uint32_t cursor_color;       // Color for cursor
    uint32_t background_color;   // Background color for input field
    bool allow_tab;              // Whether Tab key completes input
    bool allow_escape;           // Whether Escape key cancels input
    const char* allowed_chars;   // NULL = all printable chars, or string of allowed chars
    const char* forbidden_chars; // NULL = none forbidden, or string of forbidden chars
    bool auto_complete;          // Enable auto-completion (if supported)
    int cursor_blink_ms;         // Cursor blink rate in milliseconds (0 = no blink)
};

/**
 * Form field definition (legacy - use data-driven forms instead)
 */
struct FormField {
    int id;                      // Unique field identifier
    int x, y;                    // Position on screen
    int width;                   // Display width in characters
    int max_length;              // Maximum input length
    char label[64];              // Field label
    char default_value[256];     // Default/initial value
    InputOptions options;        // Input options
    bool required;               // Whether field is required
    bool (*validator)(const char* value); // Custom validation function
    char error_message[128];     // Error message for validation failures
};

/**
 * Form structure (opaque to user)
 */
struct Form;

/**
 * Data-driven form handle (opaque to user)
 * Forms are created from text with embedded @:name:type:length markers
 */
struct FormHandle;

/**
 * Field validation function type for custom validators
 */
typedef bool (*FieldValidator)(const char* field_name, const char* value, 
                              char* error_buffer, size_t error_buffer_size);

/**
 * Form parse result codes
 */
typedef enum {
    FORM_PARSE_SUCCESS,
    FORM_PARSE_NO_FIELDS,
    FORM_PARSE_INVALID_FIELD_TYPE,
    FORM_PARSE_INVALID_FIELD_LENGTH,
    FORM_PARSE_MALFORMED_MARKER,
    FORM_PARSE_INVALID_HANDLE
} FormParseResult;

/**
 * Joystick state structure
 */
struct JoystickState {
    bool connected;              // Whether joystick is connected
    char name[128];              // Joystick name
    int num_axes;                // Number of analog axes
    int num_buttons;             // Number of buttons
    int num_hats;                // Number of D-pad hats
    float axes[MAX_JOYSTICK_AXES];        // Axis values (-1.0 to 1.0)
    uint32_t buttons;            // Button state bitmask
    uint32_t buttons_pressed;    // Buttons pressed this frame
    uint32_t buttons_released;   // Buttons released this frame
};

// =============================================================================
// IMMEDIATE INPUT API (Game/Real-time)
// =============================================================================

/**
 * Check if a key is currently pressed
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
 * Check if a specific mouse button is currently pressed
 * Thread-safe, non-blocking
 * @param button Button to check (INPUT_MOUSE_LEFT, etc.)
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
 * Get mouse wheel scroll amounts since last frame
 * Thread-safe, non-blocking
 * @param wheel_x Pointer to store horizontal scroll amount
 * @param wheel_y Pointer to store vertical scroll amount
 */
void get_mouse_wheel(int* wheel_x, int* wheel_y);

// =============================================================================
// EVENT-BASED INPUT API
// =============================================================================

/**
 * Check if any input events are available
 * Thread-safe, non-blocking
 * @return true if events are available in the queue
 */
bool has_input_events();

/**
 * Get the next keyboard event from the queue
 * Thread-safe, non-blocking
 * @param event Pointer to store the event data
 * @return true if an event was retrieved, false if queue empty
 */
bool get_next_key_event(KeyEvent* event);

/**
 * Get the next mouse event from the queue
 * Thread-safe, non-blocking
 * @param event Pointer to store the event data
 * @return true if an event was retrieved, false if queue empty
 */
bool get_next_mouse_event(MouseEvent* event);

/**
 * Clear all pending input events
 * Thread-safe
 */
void clear_input_events();

/**
 * Enable or disable event queuing
 * When disabled, events are still processed for immediate input but not queued
 * @param enabled Whether to enable event queuing
 */
void set_event_queuing_enabled(bool enabled);

// =============================================================================
// TEXT INPUT API
// =============================================================================

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
 * Blocking character input with timeout
 * Thread-safe, blocking with timeout
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Key code of pressed key, or 0 if timeout occurred
 */
int waitkey_timeout(int timeout_ms);

/**
 * Basic line input with editing capabilities
 * Thread-safe, blocking until Enter/Tab pressed or cancelled
 * @param x Starting X position for input
 * @param y Starting Y position for input
 * @param max_length Maximum input length (0 = use default)
 * @return Dynamically allocated string (caller must free), or NULL if cancelled
 */
char* input_line(int x, int y, int max_length);

/**
 * Line input with prompt
 * Thread-safe, blocking
 * @param x Starting X position for prompt
 * @param y Starting Y position for prompt
 * @param prompt Prompt string to display
 * @param max_length Maximum input length
 * @return Dynamically allocated string (caller must free), or NULL if cancelled
 */
char* input_line_with_prompt(int x, int y, const char* prompt, int max_length);

/**
 * Masked line input (for passwords)
 * Thread-safe, blocking
 * @param x Starting X position for input
 * @param y Starting Y position for input
 * @param max_length Maximum input length
 * @param mask_char Character to display instead of actual input
 * @return Dynamically allocated string (caller must free), or NULL if cancelled
 */
char* input_line_masked(int x, int y, int max_length, char mask_char);

/**
 * Advanced line input with full options
 * Thread-safe, blocking
 * @param x Starting X position for input
 * @param y Starting Y position for input
 * @param prompt Prompt string to display (can be NULL)
 * @param options Input options structure
 * @return Dynamically allocated string (caller must free), or NULL if cancelled
 */
char* input_line_advanced(int x, int y, const char* prompt, const InputOptions* options);

/**
 * Set default input options for subsequent input operations
 * Thread-safe
 * @param options Default options to use
 */
void set_default_input_options(const InputOptions* options);

/**
 * Get current default input options
 * Thread-safe
 * @param options Pointer to store current default options
 */
void get_default_input_options(InputOptions* options);

// =============================================================================
// FORM INPUT API
// =============================================================================

/**
 * Create a new form
 * Thread-safe
 * @param max_fields Maximum number of fields in the form
 * @return Pointer to form structure, or NULL on failure
 */
struct Form* create_form(int max_fields);

/**
 * Add a field to a form
 * Thread-safe
 * @param form Form to add field to
 * @param field Field definition
 * @return true on success, false on failure
 */
bool add_form_field(struct Form* form, const FormField* field);

/**
 * Process form input (blocking until form completed or cancelled)
 * Thread-safe, blocking
 * @param form Form to process
 * @return true if form completed successfully, false if cancelled
 */
bool process_form(struct Form* form);

/**
 * Get the value of a specific form field
 * Thread-safe
 * @param form Form to query
 * @param field_id Field ID to get value for
 * @return Field value string, or NULL if field not found
 */
const char* get_form_field_value(struct Form* form, int field_id);

/**
 * Set the value of a specific form field
 * Thread-safe
 * @param form Form to modify
 * @param field_id Field ID to set value for
 * @param value New value for the field
 * @return true on success, false if field not found
 */
bool set_form_field_value(struct Form* form, int field_id, const char* value);

/**
 * Check if a form field has validation errors
 * Thread-safe
 * @param form Form to check
 * @param field_id Field ID to check
 * @return true if field has validation errors
 */
bool form_field_has_error(struct Form* form, int field_id);

/**
 * Get validation error message for a form field
 * Thread-safe
 * @param form Form to check
 * @param field_id Field ID to check
 * @return Error message string, or NULL if no error
 */
const char* get_form_field_error(struct Form* form, int field_id);

/**
 * Destroy a form and free its resources
 * Thread-safe
 * @param form Form to destroy
 */
void destroy_form(struct Form* form);

// =============================================================================
// DATA-DRIVEN FORMS API
// =============================================================================

/**
 * Create a form from text content with embedded field markers
 * Field markers use the format: @:field_name:type:length
 * 
 * Supported types: string, number, integer, password, email, phone, date, 
 *                  time, url, alpha, alphanum, currency, percent, zipcode, ssn
 * 
 * Example:
 *   "Name: @:first_name:string:20  @:last_name:string:25\n"
 *   "Email: @:email:email:40\n"
 *   "Age: @:age:number:3"
 * 
 * @param text_content Text containing @:field:type:length markers
 * @param x Starting X position for the form
 * @param y Starting Y position for the form
 * @return Form handle, or NULL on failure
 */
struct FormHandle* create_data_driven_form(const char* text_content, int x, int y);

/**
 * Process the form and handle user input
 * This function blocks until the user submits or cancels the form
 * 
 * Navigation:
 * - Tab: Next field
 * - Shift+Tab: Previous field
 * - Enter: Next field (or submit on last field)
 * - Ctrl+Enter: Submit from any field
 * - Escape: Cancel
 * 
 * @param form Form handle from create_data_driven_form
 * @return true if form completed successfully, false if cancelled
 */
bool process_data_driven_form(struct FormHandle* form);

/**
 * Get field value by name after form completion
 * @param form Form handle
 * @param field_name Name of the field to retrieve
 * @return Field value as string, or NULL if field not found
 */
const char* get_field_value(struct FormHandle* form, const char* field_name);

/**
 * Set field value by name (for default values or programmatic updates)
 * @param form Form handle
 * @param field_name Name of the field to set
 * @param value Value to set
 * @return true on success, false if field not found
 */
bool set_field_value(struct FormHandle* form, const char* field_name, const char* value);

/**
 * Check if a field has validation errors
 * @param form Form handle  
 * @param field_name Name of the field to check
 * @return true if field has errors
 */
bool field_has_error(struct FormHandle* form, const char* field_name);

/**
 * Get validation error message for a field
 * @param form Form handle
 * @param field_name Name of the field
 * @return Error message, or NULL if no error
 */
const char* get_field_error(struct FormHandle* form, const char* field_name);

/**
 * Check if form has any validation errors
 * @param form Form handle
 * @return true if any field has errors
 */
bool form_has_errors(struct FormHandle* form);

/**
 * Get list of all field names in a form
 * @param form Form handle
 * @param field_names Array to store field names (caller allocated)
 * @param max_fields Maximum number of fields to return
 * @return Number of fields found
 */
int get_field_names(struct FormHandle* form, const char** field_names, int max_fields);

/**
 * Register a custom validator for a specific field
 * @param form Form handle
 * @param field_name Name of the field
 * @param validator Custom validation function
 * @return true on success, false if field not found
 */
bool set_field_validator(struct FormHandle* form, const char* field_name, FieldValidator validator);

/**
 * Validate a field value according to its type
 * @param field_type Type of the field (string, number, email, etc.)
 * @param value Value to validate
 * @return true if valid, false otherwise
 */
bool validate_field_value(const char* field_type, const char* value);

/**
 * Format a field value according to its type
 * @param field_type Type of the field
 * @param raw_value Raw input value
 * @param formatted_buffer Buffer to store formatted value
 * @param buffer_size Size of the buffer
 * @return true if formatting successful
 */
bool format_field_value(struct FormHandle* form, const char* field_name);

/**
 * Get the last parse error that occurred during form creation
 * @param form Form handle
 * @return Parse error code
 */
FormParseResult get_last_parse_error(struct FormHandle* form);

/**
 * Get human-readable error message for a parse error
 * @param error Parse error code
 * @return Error message string
 */
const char* get_parse_error_message(FormParseResult error);

/**
 * Destroy form and free resources
 * @param form Form handle to destroy
 */
void destroy_data_driven_form(struct FormHandle* form);

// =============================================================================
// JOYSTICK/GAMEPAD API
// =============================================================================

/**
 * Get number of connected joysticks
 * Thread-safe, non-blocking
 * @return Number of connected joysticks
 */
int get_joystick_count();

/**
 * Check if a specific joystick is connected
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @return true if joystick is connected
 */
bool is_joystick_connected(int joystick_id);

/**
 * Get complete joystick state
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @param state Pointer to store joystick state
 * @return true if joystick exists and state retrieved
 */
bool get_joystick_state(int joystick_id, JoystickState* state);

/**
 * Get joystick axis value
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @param axis Axis number (0-based)
 * @return Axis value (-1.0 to 1.0), or 0.0 if invalid
 */
float get_joystick_axis(int joystick_id, int axis);

/**
 * Check if a joystick button is currently pressed
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @param button Button number (0-based)
 * @return true if button is pressed
 */
bool get_joystick_button(int joystick_id, int button);

/**
 * Check if a joystick button was just pressed this frame
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @param button Button number (0-based)
 * @return true if button was pressed this frame
 */
bool joystick_button_pressed(int joystick_id, int button);

/**
 * Check if a joystick button was just released this frame
 * Thread-safe, non-blocking
 * @param joystick_id Joystick ID (0-based)
 * @param button Button number (0-based)
 * @return true if button was released this frame
 */
bool joystick_button_released(int joystick_id, int button);

// =============================================================================
// INPUT SYSTEM CONTROL
// =============================================================================

/**
 * Initialize the input system
 * Must be called after runtime initialization
 * @return true on success, false on failure
 */
bool init_input_system();

/**
 * Shutdown the input system and clean up resources
 */
void shutdown_input_system();

/**
 * Update input system state (called internally by runtime)
 * This processes edge detection and manages input queues
 */
void update_input_system();

/**
 * Enable or disable text input mode
 * When enabled, SDL text input events are processed for line input
 * @param enabled Whether to enable text input mode
 */
void set_text_input_enabled(bool enabled);

/**
 * Check if text input mode is currently enabled
 * @return true if text input mode is enabled
 */
bool is_text_input_enabled();

/**
 * Set input focus rectangle (for IME support)
 * @param x X position of input area
 * @param y Y position of input area
 * @param width Width of input area
 * @param height Height of input area
 */
void set_input_focus_rect(int x, int y, int width, int height);

/**
 * Enable or disable joystick input
 * @param enabled Whether to enable joystick input
 */
void set_joystick_input_enabled(bool enabled);

/**
 * Check if joystick input is enabled
 * @return true if joystick input is enabled
 */
bool is_joystick_input_enabled();

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Convert key code to human-readable string
 * @param keycode Key code to convert
 * @return String representation of key (e.g., "Space", "Enter", "F1")
 */
const char* keycode_to_string(int keycode);

/**
 * Convert string to key code
 * @param keyname String representation of key
 * @return Key code, or INPUT_KEY_UNKNOWN if not recognized
 */
int string_to_keycode(const char* keyname);

/**
 * Convert modifier flags to human-readable string
 * @param modifiers Modifier flags
 * @param buffer Buffer to store result
 * @param buffer_size Size of buffer
 * @return Pointer to buffer with modifier string (e.g., "Ctrl+Shift")
 */
char* modifiers_to_string(uint32_t modifiers, char* buffer, int buffer_size);

/**
 * Check if a character is printable for text input
 * @param keycode Key code to check
 * @return true if key represents a printable character
 */
bool is_printable_key(int keycode);

/**
 * Get the character representation of a key code
 * @param keycode Key code
 * @param modifiers Modifier state
 * @return Character value, or 0 if not a character key
 */
char keycode_to_char(int keycode, uint32_t modifiers);


#endif // INPUT_SYSTEM_H