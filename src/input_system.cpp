#include "../include/input_system.h"
#include "../include/runtime_state.h"
#include "../include/abstract_runtime.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <chrono>
#include <thread>

// Removed using namespace to avoid conflicts

// Global runtime state access
AbstractRuntime::RuntimeState* g_runtime_state = nullptr;

// Text input state
static bool g_text_input_enabled = false;

AbstractRuntime::RuntimeState* get_runtime_state() {
    return g_runtime_state;
}

void set_runtime_state(AbstractRuntime::RuntimeState* state) {
    g_runtime_state = state;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

uint64_t get_current_timestamp() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

uint32_t convert_sdl_modifiers(uint16_t sdl_mod) {
    uint32_t modifiers = 0;
    if (sdl_mod & 0x0001) modifiers |= INPUT_MOD_LSHIFT;
    if (sdl_mod & 0x0002) modifiers |= INPUT_MOD_RSHIFT;
    if (sdl_mod & 0x0040) modifiers |= INPUT_MOD_LCTRL;
    if (sdl_mod & 0x0080) modifiers |= INPUT_MOD_RCTRL;
    if (sdl_mod & 0x0100) modifiers |= INPUT_MOD_LALT;
    if (sdl_mod & 0x0200) modifiers |= INPUT_MOD_RALT;
    if (sdl_mod & 0x0400) modifiers |= INPUT_MOD_LGUI;
    if (sdl_mod & 0x0800) modifiers |= INPUT_MOD_RGUI;
    return modifiers;
}

void queue_key_event(int keycode, bool pressed, uint16_t sdl_modifiers) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state || !state->enhanced_input.event_queuing_enabled.load()) return;
    
    AbstractRuntime::KeyEvent key_event;
    key_event.type = pressed ? AbstractRuntime::KeyEvent::KEY_DOWN : AbstractRuntime::KeyEvent::KEY_UP;
    key_event.keycode = keycode;
    key_event.modifiers = convert_sdl_modifiers(sdl_modifiers);
    key_event.timestamp = get_current_timestamp();
    key_event.text[0] = '\0';
    
    state->key_event_queue.enqueue(key_event);
}

void queue_mouse_button_event(int button, bool pressed, int x, int y, uint16_t sdl_modifiers) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state || !state->enhanced_input.event_queuing_enabled.load()) return;
    
    AbstractRuntime::MouseEvent mouse_event;
    mouse_event.type = pressed ? AbstractRuntime::MouseEvent::MOUSE_BUTTON_DOWN : AbstractRuntime::MouseEvent::MOUSE_BUTTON_UP;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.button = button + 1;
    mouse_event.wheel_x = 0;
    mouse_event.wheel_y = 0;
    mouse_event.modifiers = convert_sdl_modifiers(sdl_modifiers);
    mouse_event.timestamp = get_current_timestamp();
    
    state->mouse_event_queue.enqueue(mouse_event);
}

void queue_mouse_motion_event(int x, int y, uint16_t sdl_modifiers) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state || !state->enhanced_input.event_queuing_enabled.load()) return;
    
    AbstractRuntime::MouseEvent mouse_event;
    mouse_event.type = AbstractRuntime::MouseEvent::MOUSE_MOTION;
    mouse_event.x = x;
    mouse_event.y = y;
    mouse_event.button = 0;
    mouse_event.wheel_x = 0;
    mouse_event.wheel_y = 0;
    mouse_event.modifiers = convert_sdl_modifiers(sdl_modifiers);
    mouse_event.timestamp = get_current_timestamp();
    
    state->mouse_event_queue.enqueue(mouse_event);
}

void queue_mouse_wheel_event(int wheel_x, int wheel_y, uint16_t sdl_modifiers) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state || !state->enhanced_input.event_queuing_enabled.load()) return;
    
    AbstractRuntime::MouseEvent mouse_event;
    mouse_event.type = AbstractRuntime::MouseEvent::MOUSE_WHEEL;
    mouse_event.x = 0;
    mouse_event.y = 0;
    mouse_event.button = 0;
    mouse_event.wheel_x = wheel_x;
    mouse_event.wheel_y = wheel_y;
    mouse_event.modifiers = convert_sdl_modifiers(sdl_modifiers);
    mouse_event.timestamp = get_current_timestamp();
    
    state->mouse_event_queue.enqueue(mouse_event);
}

void update_mouse_position(int x, int y) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    state->mouse_x.store(x, std::memory_order_release);
    state->mouse_y.store(y, std::memory_order_release);
}

void update_mouse_button(int button, bool pressed) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    auto& enhanced = state->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    uint32_t button_mask = 0;
    switch (button) {
        case 0: button_mask = INPUT_MOUSE_LEFT; break;
        case 1: button_mask = INPUT_MOUSE_MIDDLE; break;
        case 2: button_mask = INPUT_MOUSE_RIGHT; break;
        case 3: button_mask = INPUT_MOUSE_X1; break;
        case 4: button_mask = INPUT_MOUSE_X2; break;
        default: return;
    }
    
    uint32_t current_buttons = state->mouse_buttons.load(std::memory_order_acquire);
    uint32_t new_buttons = pressed ? (current_buttons | button_mask) : (current_buttons & ~button_mask);
    state->mouse_buttons.store(new_buttons, std::memory_order_release);
    
    if (pressed) {
        enhanced.mouse_clicked_this_frame.fetch_or(button_mask, std::memory_order_release);
        enhanced.last_mouse_frame = current_frame;
    } else {
        enhanced.mouse_released_this_frame.fetch_or(button_mask, std::memory_order_release);
        enhanced.last_mouse_frame = current_frame;
    }
}

void update_mouse_wheel(int wheel_x, int wheel_y) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    auto& enhanced = state->enhanced_input;
    enhanced.mouse_wheel_x.store(wheel_x, std::memory_order_release);
    enhanced.mouse_wheel_y.store(wheel_y, std::memory_order_release);
}

int sdl_to_abstract_keycode(SDL_Keycode sdl_key) {
    switch (sdl_key) {
        case SDLK_SPACE: return INPUT_KEY_SPACE;
        case SDLK_RETURN: case SDLK_KP_ENTER: return INPUT_KEY_ENTER;
        case SDLK_TAB: return INPUT_KEY_TAB;
        case SDLK_ESCAPE: return INPUT_KEY_ESCAPE;
        case SDLK_BACKSPACE: return INPUT_KEY_BACKSPACE;
        case SDLK_DELETE: return INPUT_KEY_DELETE;
        case SDLK_LEFT: return INPUT_KEY_LEFT;
        case SDLK_RIGHT: return INPUT_KEY_RIGHT;
        case SDLK_UP: return INPUT_KEY_UP;
        case SDLK_DOWN: return INPUT_KEY_DOWN;
        case SDLK_HOME: return INPUT_KEY_HOME;
        case SDLK_END: return INPUT_KEY_END;
        case SDLK_PAGEUP: return INPUT_KEY_PAGE_UP;
        case SDLK_PAGEDOWN: return INPUT_KEY_PAGE_DOWN;
        case SDLK_INSERT: return INPUT_KEY_INSERT;
        case SDLK_F1: return INPUT_KEY_F1;
        case SDLK_F2: return INPUT_KEY_F2;
        case SDLK_F3: return INPUT_KEY_F3;
        case SDLK_F4: return INPUT_KEY_F4;
        case SDLK_F5: return INPUT_KEY_F5;
        case SDLK_F6: return INPUT_KEY_F6;
        case SDLK_F7: return INPUT_KEY_F7;
        case SDLK_F8: return INPUT_KEY_F8;
        case SDLK_F9: return INPUT_KEY_F9;
        case SDLK_F10: return INPUT_KEY_F10;
        case SDLK_F11: return INPUT_KEY_F11;
        case SDLK_F12: return INPUT_KEY_F12;
        case SDLK_LSHIFT: return INPUT_KEY_LSHIFT;
        case SDLK_RSHIFT: return INPUT_KEY_RSHIFT;
        case SDLK_LCTRL: return INPUT_KEY_LCTRL;
        case SDLK_RCTRL: return INPUT_KEY_RCTRL;
        case SDLK_LALT: return INPUT_KEY_LALT;
        case SDLK_RALT: return INPUT_KEY_RALT;
        case SDLK_LGUI: return INPUT_KEY_LGUI;
        case SDLK_RGUI: return INPUT_KEY_RGUI;
        default:
            if (sdl_key >= 32 && sdl_key <= 126) {
                return sdl_key;
            }
            return INPUT_KEY_UNKNOWN;
    }
}

uint32_t sdl_to_abstract_modifiers(uint16_t sdl_mod) {
    uint32_t modifiers = INPUT_MOD_NONE;
    
    if (sdl_mod & KMOD_LSHIFT) modifiers |= INPUT_MOD_LSHIFT;
    if (sdl_mod & KMOD_RSHIFT) modifiers |= INPUT_MOD_RSHIFT;
    if (sdl_mod & KMOD_LCTRL) modifiers |= INPUT_MOD_LCTRL;
    if (sdl_mod & KMOD_RCTRL) modifiers |= INPUT_MOD_RCTRL;
    if (sdl_mod & KMOD_LALT) modifiers |= INPUT_MOD_LALT;
    if (sdl_mod & KMOD_RALT) modifiers |= INPUT_MOD_RALT;
    if (sdl_mod & KMOD_LGUI) modifiers |= INPUT_MOD_LGUI;
    if (sdl_mod & KMOD_RGUI) modifiers |= INPUT_MOD_RGUI;
    
    return modifiers;
}

// =============================================================================
// IMMEDIATE INPUT API
// =============================================================================

bool is_key_pressed(int keycode) {
    if (keycode < 0 || keycode >= MAX_INPUT_KEYS) {
        return false;
    }
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    return state->key_states[keycode].load(std::memory_order_acquire);
}

bool key_just_pressed(int keycode) {
    if (keycode < 0 || keycode >= MAX_INPUT_KEYS) {
        return false;
    }
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    auto& enhanced = state->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    return enhanced.key_pressed_this_frame[keycode].load(std::memory_order_acquire) &&
           enhanced.last_key_frame[keycode] == current_frame;
}

bool key_just_released(int keycode) {
    if (keycode < 0 || keycode >= MAX_INPUT_KEYS) {
        return false;
    }
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    auto& enhanced = state->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    return enhanced.key_released_this_frame[keycode].load(std::memory_order_acquire) &&
           enhanced.last_key_frame[keycode] == current_frame;
}

uint32_t get_modifier_state() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return INPUT_MOD_NONE;
    
    return state->enhanced_input.modifier_state.load(std::memory_order_acquire);
}

int get_mouse_x() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    return state->mouse_x.load(std::memory_order_acquire);
}

int get_mouse_y() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    return state->mouse_y.load(std::memory_order_acquire);
}

uint32_t get_mouse_buttons() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    return state->mouse_buttons.load(std::memory_order_acquire);
}

bool is_mouse_button_pressed(int button) {
    uint32_t buttons = get_mouse_buttons();
    return (buttons & (1 << button)) != 0;
}

bool mouse_button_clicked(int button) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    auto& enhanced = state->enhanced_input;
    uint32_t clicked = enhanced.mouse_clicked_this_frame.load(std::memory_order_acquire);
    return (clicked & (1 << button)) != 0;
}

void get_mouse_wheel(int* wheel_x, int* wheel_y) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) {
        if (wheel_x) *wheel_x = 0;
        if (wheel_y) *wheel_y = 0;
        return;
    }
    
    auto& enhanced = state->enhanced_input;
    if (wheel_x) *wheel_x = enhanced.mouse_wheel_x.load(std::memory_order_acquire);
    if (wheel_y) *wheel_y = enhanced.mouse_wheel_y.load(std::memory_order_acquire);
}

// =============================================================================
// EVENT-BASED INPUT API
// =============================================================================

bool has_input_events() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    return !state->key_event_queue.empty() || !state->mouse_event_queue.empty();
}

bool get_next_key_event(KeyEvent* event) {
    if (!event) return false;
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    AbstractRuntime::KeyEvent internal_event;
    if (state->key_event_queue.dequeue(internal_event)) {
        event->type = (KeyEvent::Type)internal_event.type;
        event->keycode = internal_event.keycode;
        event->modifiers = internal_event.modifiers;
        event->timestamp = internal_event.timestamp;
        strncpy(event->text, internal_event.text, sizeof(event->text) - 1);
        event->text[sizeof(event->text) - 1] = '\0';
        return true;
    }
    return false;
}

bool get_next_mouse_event(MouseEvent* event) {
    if (!event) return false;
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    AbstractRuntime::MouseEvent internal_event;
    if (state->mouse_event_queue.dequeue(internal_event)) {
        event->type = (MouseEvent::Type)internal_event.type;
        event->x = internal_event.x;
        event->y = internal_event.y;
        event->button = internal_event.button;
        event->wheel_x = internal_event.wheel_x;
        event->wheel_y = internal_event.wheel_y;
        event->modifiers = internal_event.modifiers;
        event->timestamp = internal_event.timestamp;
        return true;
    }
    return false;
}

void clear_input_events() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    AbstractRuntime::KeyEvent key_event;
    AbstractRuntime::MouseEvent mouse_event;
    
    while (state->key_event_queue.dequeue(key_event)) { }
    while (state->mouse_event_queue.dequeue(mouse_event)) { }
}

void set_event_queuing_enabled(bool enabled) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    state->enhanced_input.event_queuing_enabled.store(enabled, std::memory_order_release);
}

// =============================================================================
// BASIC TEXT INPUT API
// =============================================================================

int inkey() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    AbstractRuntime::InputEvent event;
    if (state->input_queue.dequeue(event)) {
        if (event.type == AbstractRuntime::INPUT_KEY_PRESS) {
            return event.keycode;
        }
    }
    
    return 0;
}

int waitkey() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    AbstractRuntime::InputEvent event;
    
    while (!state->shutdown_requested.load(std::memory_order_acquire)) {
        if (state->input_queue.dequeue(event)) {
            if (event.type == AbstractRuntime::INPUT_KEY_PRESS) {
                return event.keycode;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return INPUT_KEY_ESCAPE;
}

int waitkey_timeout(int timeout_ms) {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    
    AbstractRuntime::InputEvent event;
    
    while (!state->shutdown_requested.load(std::memory_order_acquire)) {
        if (state->input_queue.dequeue(event)) {
            if (event.type == AbstractRuntime::INPUT_KEY_PRESS) {
                return event.keycode;
            }
        }
        
        if (timeout_ms > 0) {
            auto current_time = std::chrono::high_resolution_clock::now();
            if (current_time - start_time >= timeout_duration) {
                return 0;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return INPUT_KEY_ESCAPE;
}

// =============================================================================
// INPUT SYSTEM CONTROL
// =============================================================================

bool init_input_system() {
    if (!g_runtime_state) {
        g_runtime_state = new AbstractRuntime::RuntimeState();
    }
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return false;
    
    auto& enhanced = state->enhanced_input;
    
    enhanced.current_frame.store(0, std::memory_order_release);
    
    for (int i = 0; i < MAX_INPUT_KEYS; ++i) {
        enhanced.key_pressed_this_frame[i].store(false, std::memory_order_release);
        enhanced.key_released_this_frame[i].store(false, std::memory_order_release);
        enhanced.last_key_frame[i] = 0;
    }
    
    enhanced.mouse_clicked_this_frame.store(0, std::memory_order_release);
    enhanced.mouse_released_this_frame.store(0, std::memory_order_release);
    enhanced.mouse_wheel_x.store(0, std::memory_order_release);
    enhanced.mouse_wheel_y.store(0, std::memory_order_release);
    enhanced.last_mouse_frame = 0;
    
    enhanced.event_queuing_enabled.store(true, std::memory_order_release);
    
    // Initialize text input state
    g_text_input_enabled = false;
    
    return true;
}

void shutdown_input_system() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    // Disable text input if it was enabled
    if (is_text_input_enabled()) {
        set_text_input_enabled(false);
    }
    
    clear_input_events();
    
    AbstractRuntime::InputEvent event;
    while (state->input_queue.dequeue(event)) { }
    
    delete g_runtime_state;
    g_runtime_state = nullptr;
}

void update_input_system() {
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    auto& enhanced = state->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.fetch_add(1, std::memory_order_acq_rel) + 1;
    
    for (int i = 0; i < MAX_INPUT_KEYS; ++i) {
        if (enhanced.last_key_frame[i] < current_frame - 1) {
            enhanced.key_pressed_this_frame[i].store(false, std::memory_order_release);
            enhanced.key_released_this_frame[i].store(false, std::memory_order_release);
        }
    }
    
    if (enhanced.last_mouse_frame < current_frame - 1) {
        enhanced.mouse_clicked_this_frame.store(0, std::memory_order_release);
        enhanced.mouse_released_this_frame.store(0, std::memory_order_release);
    }
    
    enhanced.mouse_wheel_x.store(0, std::memory_order_release);
    enhanced.mouse_wheel_y.store(0, std::memory_order_release);
}

void update_key_state(int keycode, bool pressed) {
    if (keycode < 0 || keycode >= MAX_INPUT_KEYS) {
        return;
    }
    
    AbstractRuntime::RuntimeState* state = get_runtime_state();
    if (!state) return;
    
    state->key_states[keycode].store(pressed, std::memory_order_release);
    
    auto& enhanced = state->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    if (pressed) {
        enhanced.key_pressed_this_frame[keycode].store(true, std::memory_order_release);
        enhanced.last_key_frame[keycode] = current_frame;
    } else {
        enhanced.key_released_this_frame[keycode].store(true, std::memory_order_release);
        enhanced.last_key_frame[keycode] = current_frame;
    }
}

void update_key_state_with_event(int keycode, bool pressed, uint16_t sdl_modifiers) {
    update_key_state(keycode, pressed);
    queue_key_event(keycode, pressed, sdl_modifiers);
}

void update_mouse_button_with_event(int button, bool pressed, int x, int y, uint16_t sdl_modifiers) {
    update_mouse_button(button, pressed);
    queue_mouse_button_event(button, pressed, x, y, sdl_modifiers);
}

void update_mouse_position_with_event(int x, int y, uint16_t sdl_modifiers) {
    update_mouse_position(x, y);
    queue_mouse_motion_event(x, y, sdl_modifiers);
}

void update_mouse_wheel_with_event(int wheel_x, int wheel_y, uint16_t sdl_modifiers) {
    update_mouse_wheel(wheel_x, wheel_y);
    queue_mouse_wheel_event(wheel_x, wheel_y, sdl_modifiers);
}

// Runtime text input update functions (will be implemented by accept_at.cpp)
void update_runtime_text_input() __attribute__((weak));
void process_runtime_text_input_events() __attribute__((weak));
void process_char_input_events() __attribute__((weak));
void update_runtime_form_input() __attribute__((weak));
void process_runtime_form_input_events() __attribute__((weak));

void update_runtime_text_input() {
    // Weak symbol - implemented in accept_at.cpp
}

void process_runtime_text_input_events() {
    // Weak symbol - implemented in accept_at.cpp  
}

void process_char_input_events() {
    // Weak symbol - implemented in accept_at.cpp
}

void update_runtime_form_input() {
    // Weak symbol - implemented in data_driven_forms.cpp
}

void process_runtime_form_input_events() {
    // Weak symbol - implemented in data_driven_forms.cpp
}

// =============================================================================
// TEXT INPUT CONTROL FUNCTIONS
// =============================================================================

void set_text_input_enabled(bool enabled) {
    if (enabled != g_text_input_enabled) {
        g_text_input_enabled = enabled;
        if (enabled) {
            SDL_StartTextInput();
        } else {
            SDL_StopTextInput();
        }
    }
}

bool is_text_input_enabled() {
    return g_text_input_enabled;
}

