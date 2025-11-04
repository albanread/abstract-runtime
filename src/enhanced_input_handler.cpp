/*
 * UNUSED CODE - OLD ARCHITECTURE
 * 
 * This Enhanced Input Handler was designed for an architecture where:
 * - Main thread ran the application
 * - Background thread ran the compositor/renderer
 * - Events were processed by CompositorThread and handled here
 * 
 * Current architecture (macOS compatible):
 * - Main thread runs the runtime/renderer (required for macOS UI events)
 * - Background thread runs the application
 * - Events processed directly on main thread in abstract_runtime.cpp
 * 
 * This file is kept for reference but is not used in the current system.
 */

#include "../include/compositor_thread.h"
#include "../include/runtime_state.h"
#include "../include/input_system.h"
#include <SDL2/SDL.h>
#include <chrono>

using namespace AbstractRuntime;

// External function to convert SDL keycode to abstract keycode
extern int sdl_to_abstract_keycode(SDL_Keycode sdl_key);
extern uint32_t sdl_to_abstract_modifiers(uint16_t sdl_mod);

/**
 * Enhanced key event handler with edge detection and event queuing
 */
void CompositorThread::handle_enhanced_key_event(const SDL_KeyboardEvent& key_event, bool pressed) {
    int keycode = sdl_to_abstract_keycode(key_event.keysym.sym);
    if (keycode == INPUT_KEY_UNKNOWN) {
        return; // Ignore unknown keys
    }
    
    auto& enhanced = runtime_state_->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    // Update current key state
    bool was_pressed = runtime_state_->key_states[keycode].load(std::memory_order_acquire);
    runtime_state_->key_states[keycode].store(pressed, std::memory_order_release);
    
    // Update edge detection flags
    if (pressed && !was_pressed) {
        // Key was just pressed
        enhanced.key_pressed_this_frame[keycode].store(true, std::memory_order_release);
        enhanced.key_released_this_frame[keycode].store(false, std::memory_order_release);
        enhanced.last_key_frame[keycode] = current_frame;
    } else if (!pressed && was_pressed) {
        // Key was just released
        enhanced.key_pressed_this_frame[keycode].store(false, std::memory_order_release);
        enhanced.key_released_this_frame[keycode].store(true, std::memory_order_release);
        enhanced.last_key_frame[keycode] = current_frame;
    }
    
    // Update modifier state
    uint32_t modifiers = sdl_to_abstract_modifiers(key_event.keysym.mod);
    enhanced.modifier_state.store(modifiers, std::memory_order_release);
    
    // Add to legacy input queue for WAITKEY/INKEY (only on press)
    if (pressed) {
        InputEvent input_event = {
            .type = INPUT_KEY_PRESS,
            .keycode = keycode,
            .timestamp = get_ticks()
        };
        runtime_state_->input_queue.enqueue(input_event);
    }
    
    // Add to enhanced key event queue if enabled
    if (enhanced.event_queuing_enabled.load(std::memory_order_acquire)) {
        KeyEvent key_event_enhanced;
        key_event_enhanced.type = pressed ? KeyEvent::KEY_DOWN : KeyEvent::KEY_UP;
        key_event_enhanced.keycode = keycode;
        key_event_enhanced.modifiers = modifiers;
        key_event_enhanced.timestamp = get_ticks();
        
        // Copy text input if available (SDL provides this for text events)
        if (pressed && key_event.keysym.sym >= 32 && key_event.keysym.sym <= 126) {
            key_event_enhanced.text[0] = (char)key_event.keysym.sym;
            key_event_enhanced.text[1] = '\0';
        } else {
            key_event_enhanced.text[0] = '\0';
        }
        
        runtime_state_->key_event_queue.enqueue(key_event_enhanced);
    }
}

/**
 * Enhanced mouse button event handler with edge detection
 */
void CompositorThread::handle_enhanced_mouse_button_event(const SDL_MouseButtonEvent& button_event, bool pressed) {
    uint32_t button_mask = 0;
    int button_num = 0;
    
    switch (button_event.button) {
        case SDL_BUTTON_LEFT:   
            button_mask = INPUT_MOUSE_LEFT; 
            button_num = 1; 
            break;
        case SDL_BUTTON_RIGHT:  
            button_mask = INPUT_MOUSE_RIGHT; 
            button_num = 3; 
            break;
        case SDL_BUTTON_MIDDLE: 
            button_mask = INPUT_MOUSE_MIDDLE; 
            button_num = 2; 
            break;
        case SDL_BUTTON_X1:     
            button_mask = INPUT_MOUSE_X1; 
            button_num = 4; 
            break;
        case SDL_BUTTON_X2:     
            button_mask = INPUT_MOUSE_X2; 
            button_num = 5; 
            break;
        default:
            return; // Unknown button
    }
    
    auto& enhanced = runtime_state_->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.load(std::memory_order_acquire);
    
    // Update mouse button state
    uint32_t current_buttons = runtime_state_->mouse_buttons.load(std::memory_order_acquire);
    uint32_t new_buttons = pressed ? (current_buttons | button_mask) : (current_buttons & ~button_mask);
    runtime_state_->mouse_buttons.store(new_buttons, std::memory_order_release);
    
    // Update edge detection for mouse clicks
    if (pressed) {
        enhanced.mouse_clicked_this_frame.fetch_or(button_mask, std::memory_order_release);
        enhanced.last_mouse_frame = current_frame;
    } else {
        enhanced.mouse_released_this_frame.fetch_or(button_mask, std::memory_order_release);
        enhanced.last_mouse_frame = current_frame;
    }
    
    // Track legacy button clicks for mouse_clicked() function
    if (pressed) {
        runtime_state_->mouse_clicked_flags.fetch_or(button_mask, std::memory_order_release);
    }
    
    // Add to mouse event queue if enabled
    if (enhanced.event_queuing_enabled.load(std::memory_order_acquire)) {
        MouseEvent mouse_event;
        mouse_event.type = pressed ? MouseEvent::MOUSE_BUTTON_DOWN : MouseEvent::MOUSE_BUTTON_UP;
        mouse_event.x = button_event.x;
        mouse_event.y = button_event.y;
        mouse_event.button = button_num;
        mouse_event.wheel_x = 0;
        mouse_event.wheel_y = 0;
        mouse_event.modifiers = sdl_to_abstract_modifiers(SDL_GetModState());
        mouse_event.timestamp = get_ticks();
        
        runtime_state_->mouse_event_queue.enqueue(mouse_event);
    }
}

/**
 * Enhanced mouse motion event handler
 */
void CompositorThread::handle_enhanced_mouse_motion_event(const SDL_MouseMotionEvent& motion_event) {
    // Update mouse position
    runtime_state_->mouse_x.store(motion_event.x, std::memory_order_release);
    runtime_state_->mouse_y.store(motion_event.y, std::memory_order_release);
    
    auto& enhanced = runtime_state_->enhanced_input;
    
    // Add to mouse event queue if enabled
    if (enhanced.event_queuing_enabled.load(std::memory_order_acquire)) {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::MOUSE_MOTION;
        mouse_event.x = motion_event.x;
        mouse_event.y = motion_event.y;
        mouse_event.button = 0;
        mouse_event.wheel_x = 0;
        mouse_event.wheel_y = 0;
        mouse_event.modifiers = sdl_to_abstract_modifiers(SDL_GetModState());
        mouse_event.timestamp = get_ticks();
        
        runtime_state_->mouse_event_queue.enqueue(mouse_event);
    }
}

/**
 * Enhanced mouse wheel event handler
 */
void CompositorThread::handle_enhanced_mouse_wheel_event(const SDL_MouseWheelEvent& wheel_event) {
    auto& enhanced = runtime_state_->enhanced_input;
    
    // Accumulate wheel motion (per-frame)
    int current_wheel_x = enhanced.mouse_wheel_x.load(std::memory_order_acquire);
    int current_wheel_y = enhanced.mouse_wheel_y.load(std::memory_order_acquire);
    
    enhanced.mouse_wheel_x.store(current_wheel_x + wheel_event.x, std::memory_order_release);
    enhanced.mouse_wheel_y.store(current_wheel_y + wheel_event.y, std::memory_order_release);
    
    // Add to mouse event queue if enabled
    if (enhanced.event_queuing_enabled.load(std::memory_order_acquire)) {
        MouseEvent mouse_event;
        mouse_event.type = MouseEvent::MOUSE_WHEEL;
        mouse_event.x = runtime_state_->mouse_x.load(std::memory_order_acquire);
        mouse_event.y = runtime_state_->mouse_y.load(std::memory_order_acquire);
        mouse_event.button = 0;
        mouse_event.wheel_x = wheel_event.x;
        mouse_event.wheel_y = wheel_event.y;
        mouse_event.modifiers = sdl_to_abstract_modifiers(SDL_GetModState());
        mouse_event.timestamp = get_ticks();
        
        runtime_state_->mouse_event_queue.enqueue(mouse_event);
    }
}

/**
 * Enhanced event processing with new handlers
 */
void CompositorThread::process_enhanced_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            running_.store(false);
            break;

        case SDL_KEYDOWN:
            handle_enhanced_key_event(event.key, true);
            break;

        case SDL_KEYUP:
            handle_enhanced_key_event(event.key, false);
            break;

        case SDL_MOUSEBUTTONDOWN:
            handle_enhanced_mouse_button_event(event.button, true);
            break;

        case SDL_MOUSEBUTTONUP:
            handle_enhanced_mouse_button_event(event.button, false);
            break;

        case SDL_MOUSEMOTION:
            handle_enhanced_mouse_motion_event(event.motion);
            break;

        case SDL_MOUSEWHEEL:
            handle_enhanced_mouse_wheel_event(event.wheel);
            break;

        case SDL_WINDOWEVENT:
            handle_window_event(event.window);
            break;

        default:
            // Ignore other events
            break;
    }
}

/**
 * Update input system state each frame
 */
void CompositorThread::update_enhanced_input_frame() {
    auto& enhanced = runtime_state_->enhanced_input;
    uint64_t current_frame = enhanced.current_frame.fetch_add(1, std::memory_order_acq_rel) + 1;
    
    // Clear edge detection flags for events older than current frame
    for (int i = 0; i < MAX_INPUT_KEYS; ++i) {
        if (enhanced.last_key_frame[i] < current_frame - 1) {
            enhanced.key_pressed_this_frame[i].store(false, std::memory_order_release);
            enhanced.key_released_this_frame[i].store(false, std::memory_order_release);
        }
    }
    
    // Clear mouse edge detection if old
    if (enhanced.last_mouse_frame < current_frame - 1) {
        enhanced.mouse_clicked_this_frame.store(0, std::memory_order_release);
        enhanced.mouse_released_this_frame.store(0, std::memory_order_release);
    }
    
    // Reset mouse wheel deltas (they're per-frame)
    enhanced.mouse_wheel_x.store(0, std::memory_order_release);
    enhanced.mouse_wheel_y.store(0, std::memory_order_release);
    
    // Clear legacy mouse clicked flags
    runtime_state_->mouse_clicked_flags.store(0, std::memory_order_release);
}

/**
 * Initialize enhanced input system
 */
void CompositorThread::initialize_enhanced_input() {
    auto& enhanced = runtime_state_->enhanced_input;
    
    // Reset frame counter
    enhanced.current_frame.store(0, std::memory_order_release);
    
    // Clear all edge detection flags
    for (int i = 0; i < MAX_INPUT_KEYS; ++i) {
        enhanced.key_pressed_this_frame[i].store(false, std::memory_order_release);
        enhanced.key_released_this_frame[i].store(false, std::memory_order_release);
        enhanced.last_key_frame[i] = 0;
    }
    
    // Reset mouse state
    enhanced.mouse_clicked_this_frame.store(0, std::memory_order_release);
    enhanced.mouse_released_this_frame.store(0, std::memory_order_release);
    enhanced.mouse_wheel_x.store(0, std::memory_order_release);
    enhanced.mouse_wheel_y.store(0, std::memory_order_release);
    enhanced.modifier_state.store(INPUT_MOD_NONE, std::memory_order_release);
    enhanced.last_mouse_frame = 0;
    
    // Enable event queuing by default
    enhanced.event_queuing_enabled.store(true, std::memory_order_release);
}