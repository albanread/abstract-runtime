/*
 * UNUSED CODE - OLD ARCHITECTURE
 * 
 * This CompositorThread was designed for an architecture where:
 * - Main thread ran the application
 * - Background thread ran the compositor/renderer
 * - Events were forwarded via command queue
 * 
 * Current architecture (macOS compatible):
 * - Main thread runs the runtime/renderer (required for macOS UI events)
 * - Background thread runs the application
 * - Events processed directly on main thread
 * 
 * This file is kept for reference but is not used in the current system.
 * The main thread loop in abstract_runtime.cpp now handles all SDL events directly.
 */

#include "compositor_thread.h"
#include "command_queue.h"
#include "runtime_state.h"
#include "layer_renderer.h"
#include "sprite_renderer.h"
#include <SDL2/SDL.h>
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <cstdio>
#include <string>

namespace AbstractRuntime {

CompositorThread::CompositorThread(AbstractRuntime::RuntimeState* state)
    : runtime_state_(state)
    , running_(false)
    , target_fps_(60)
    , frame_time_ms_(1000.0f / target_fps_)
    , sdl_window_(nullptr)
    , gl_context_(nullptr)
    , layer_renderer_(nullptr)
    , owns_context_(false)
    , command_queue_(nullptr) {
}

CompositorThread::~CompositorThread() {
    stop();
}

bool CompositorThread::initialize(int screen_mode) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Get screen dimensions based on mode
    int width, height;
    get_screen_dimensions(screen_mode, width, height);
    screen_width_ = width;
    screen_height_ = height;

    // Set OpenGL attributes for compatibility rendering (supports immediate mode)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    sdl_window_ = SDL_CreateWindow(
        "NewBCPL Graphics Terminal",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!sdl_window_) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return false;
    }

    // Make sure window is visible and focused on macOS
    SDL_RaiseWindow(sdl_window_);
    SDL_SetWindowInputFocus(sdl_window_);

    // Create OpenGL context
    gl_context_ = SDL_GL_CreateContext(sdl_window_);
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }

    // Enable vsync for smooth rendering
    SDL_GL_SetSwapInterval(1);

    // Initialize OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, width, height);

    // Create layer renderer
    layer_renderer_ = std::make_unique<AbstractRuntime::LayerRenderer>();
    if (!layer_renderer_->initialize(width, height)) {
        std::cerr << "Failed to initialize layer renderer" << std::endl;
        return false;
    }

    // Initialize layers in runtime state
    initialize_layers();

    // Initialize enhanced input system
    initialize_enhanced_input();

    owns_context_ = true;  // We created the context
    std::cout << "Compositor initialized: " << width << "x" << height << std::endl;
    return true;
}

bool CompositorThread::initialize_with_existing_context(SDL_Window* window, SDL_GLContext context, int width, int height) {
    // Store the existing window and context
    sdl_window_ = window;
    gl_context_ = context;
    screen_width_ = width;
    screen_height_ = height;
    owns_context_ = false;  // We don't own the context

    // Make the context current on this thread
    if (SDL_GL_MakeCurrent(sdl_window_, gl_context_) != 0) {
        std::cerr << "Failed to make OpenGL context current: " << SDL_GetError() << std::endl;
        return false;
    }

    // Enable vsync for smooth rendering
    SDL_GL_SetSwapInterval(1);

    // Initialize OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, width, height);

    // Create layer renderer
    layer_renderer_ = std::make_unique<AbstractRuntime::LayerRenderer>();
    if (!layer_renderer_->initialize(width, height)) {
        std::cerr << "Failed to initialize layer renderer" << std::endl;
        return false;
    }

    // Initialize layers in runtime state
    initialize_layers();

    // Initialize enhanced input system
    initialize_enhanced_input();

    std::cout << "Compositor initialized with existing context: " << width << "x" << height << std::endl;
    return true;
}

void CompositorThread::set_command_queue(CommandQueue* queue) {
    command_queue_ = queue;
}

void CompositorThread::request_quit() {
    running_.store(false);
}

void CompositorThread::start() {
    if (running_) return;
    
    running_ = true;
    compositor_thread_ = std::thread(&CompositorThread::render_loop, this);
    std::cout << "Compositor thread started at " << target_fps_ << " fps" << std::endl;
}

void CompositorThread::stop() {
    if (!running_) return;

    std::cout << "Stopping compositor thread..." << std::endl;
    running_ = false;
    
    if (compositor_thread_.joinable()) {
        std::cout << "Waiting for compositor thread to join..." << std::endl;
        compositor_thread_.join();
        std::cout << "Compositor thread joined successfully" << std::endl;
    }

    std::cout << "Cleaning up compositor resources..." << std::endl;
    cleanup();
    std::cout << "Compositor thread stopped" << std::endl;
}

void CompositorThread::render_loop() {
    // Frame timing tracking removed - using simple frame counting now
    std::cout << "DEBUG: Render loop started" << std::endl;
    
    while (running_) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // Drain and process events from main thread command queue
        drain_and_process_events();
        
        // Update enhanced input system frame state
        update_enhanced_input_frame();
        
        // Debug: Check layer states before rendering
        if (frame_count_ % 60 == 0) {
            for (int i = 0; i < NUM_LAYERS; ++i) {
                AbstractRuntime::Layer& layer = runtime_state_->layers[i];
                std::cout << "Frame " << frame_count_ << " Layer " << i << ": visible=" << layer.visible << ", dirty=" << layer.dirty << std::endl;
            }
        }
        
        // Always render frame for debugging (remove dirty check temporarily)
        render_frame();
        
        // Calculate frame timing
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame_end - frame_start).count();
        
        // Sleep to maintain target fps
        if (frame_duration < frame_time_ms_) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(frame_time_ms_ - frame_duration))
            );
        }
        
        // Update performance stats
        update_performance_stats(frame_duration);
    }
}

void CompositorThread::render_frame() {
    // Clear with background color (like working demo)
    auto bg_color = runtime_state_->background_color.load();
    float r = bg_color.r / 255.0f;
    float g = bg_color.g / 255.0f; 
    float b = bg_color.b / 255.0f;
    
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw a test triangle to prove rendering works
    glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex2f(-0.3f, -0.3f);
        glVertex2f( 0.3f, -0.3f);
        glVertex2f( 0.0f,  0.3f);
    glEnd();
    
    // Draw simple "text" rectangles to simulate text rendering
    glColor3f(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 20; i++) {
        float x = -0.8f + i * 0.08f;
        glBegin(GL_QUADS);
            glVertex2f(x, 0.5f);
            glVertex2f(x + 0.06f, 0.5f);
            glVertex2f(x + 0.06f, 0.6f);
            glVertex2f(x, 0.6f);
        glEnd();
    }

    // Render FPS overlay at top-right
    render_fps_overlay();

    // Present frame
    SDL_GL_SwapWindow(sdl_window_);
    
    // Update frame counter
    frame_count_++;
}

void CompositorThread::render_layer(int layer_id) {
    AbstractRuntime::Layer& layer = runtime_state_->layers[layer_id];
    
    switch (layer.type) {
        case LAYER_BACKGROUND:
            render_background_layer(layer);
            break;
            
        case LAYER_TILES2:
        case LAYER_TILES1:
            render_tile_layer(layer);
            break;
            
        case LAYER_GRAPHICS:
            render_graphics_layer(layer);
            break;
            
        case LAYER_SPRITES:
            render_sprite_layer(layer);
            break;
            
        case LAYER_TEXT:
            render_text_layer(layer);
            break;
    }
}

void CompositorThread::render_background_layer(const AbstractRuntime::Layer& /* layer */) {
    std::cout << "DEBUG: render_background_layer called, sprite_slot=" << runtime_state_->background_sprite_slot.load() << std::endl;
    if (runtime_state_->background_sprite_slot.load() >= 0) {
        // Render sprite as background
        layer_renderer_->render_background_sprite(
            runtime_state_->background_sprite_slot.load(),
            screen_width_, screen_height_
        );
    } else {
        // Render solid color background as a rectangle to test if OpenGL rendering works
        auto bg_color = runtime_state_->background_color.load();
        float r = bg_color.r / 255.0f;
        float g = bg_color.g / 255.0f; 
        float b = bg_color.b / 255.0f;
        
        std::cout << "DEBUG: Rendering background rectangle RGB(" << r << ", " << g << ", " << b << ")" << std::endl;
        
        // Set up orthographic projection for screen coordinates
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        
        // Use immediate mode OpenGL to draw fullscreen background rectangle
        glDisable(GL_TEXTURE_2D);
        glColor4f(r, g, b, 1.0f);
        
        glBegin(GL_QUADS);
            glVertex2f(0, 0);                            // Top left
            glVertex2f(screen_width_, 0);                // Top right
            glVertex2f(screen_width_, screen_height_);   // Bottom right
            glVertex2f(0, screen_height_);               // Bottom left
        glEnd();
        
        std::cout << "DEBUG: Background rectangle rendered" << std::endl;
    }
}

void CompositorThread::render_tile_layer(const AbstractRuntime::Layer& layer) {
    layer_renderer_->render_tiles(
        layer.tile_data,
        layer.viewport_offset,
        screen_width_, screen_height_,
        *runtime_state_->sprite_bank
    );
}

void CompositorThread::render_graphics_layer(const AbstractRuntime::Layer& layer) {
    layer_renderer_->render_graphics(
        layer.graphics_commands,
        layer.viewport_offset,
        screen_width_, screen_height_
    );
}

void CompositorThread::render_sprite_layer(const AbstractRuntime::Layer& layer) {
    // Get current sprite positions (lock-free read)
    AbstractRuntime::SpriteInstance visible_sprites[MAX_SPRITES];
    int sprite_count = 0;
    
    for (int i = 0; i < MAX_SPRITES; ++i) {
        AbstractRuntime::SpritePosition pos = runtime_state_->sprite_positions[i].load(std::memory_order_acquire);
        if (pos.visible) {
            // Transform to screen coordinates
            int screen_x = pos.x - layer.viewport_offset.x;
            int screen_y = pos.y - layer.viewport_offset.y;
            
            // Cull sprites outside screen bounds (with sprite size margin)
            if (screen_x > -SPRITE_SIZE && screen_x < screen_width_ + SPRITE_SIZE &&
                screen_y > -SPRITE_SIZE && screen_y < screen_height_ + SPRITE_SIZE) {
                
                visible_sprites[sprite_count] = {
                    .active = true,
                    .sprite_slot = pos.sprite_slot,
                    .x = (float)screen_x,
                    .y = (float)screen_y,
                    .scale_x = pos.scale,
                    .scale_y = pos.scale,
                    .rotation = pos.rotation,
                    .alpha = pos.alpha,
                    .z_order = 0
                };
                sprite_count++;
            }
        }
    }
    
    // Render visible sprites
    layer_renderer_->render_sprites(
        visible_sprites, sprite_count, *runtime_state_->sprite_bank
    );
}

void CompositorThread::render_text_layer(const AbstractRuntime::Layer& layer) {
    std::cout << "DEBUG: Rendering text layer, dirty=" << layer.dirty << ", cols=" << layer.text_data.cols << ", rows=" << layer.text_data.rows << std::endl;
    layer_renderer_->render_text(
        layer.text_data,
        layer.viewport_offset,
        screen_width_, screen_height_,
        *runtime_state_->font_atlas
    );
}

bool CompositorThread::check_dirty_layers() {
    for (int i = 0; i < NUM_LAYERS; ++i) {
        if (runtime_state_->layers[i].dirty) {
            return true;
        }
    }
    return false;
}

void CompositorThread::drain_and_process_events() {
    if (!command_queue_) {
        return;  // No command queue set up yet
    }

    // Drain all events from the command queue into our local buffer
    command_queue_->drain_events(frame_events_);
    
    // Process all events that were queued this frame
    for (const SDL_Event& event : frame_events_) {
        process_event(event);
    }
}

void CompositorThread::process_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            running_.store(false);
            break;

        case SDL_KEYDOWN:
            handle_key_event(event.key, true);
            handle_enhanced_key_event(event.key, true);
            break;

        case SDL_KEYUP:
            handle_key_event(event.key, false);
            handle_enhanced_key_event(event.key, false);
            break;

        case SDL_MOUSEBUTTONDOWN:
            handle_mouse_button_event(event.button, true);
            handle_enhanced_mouse_button_event(event.button, true);
            break;

        case SDL_MOUSEBUTTONUP:
            handle_mouse_button_event(event.button, false);
            handle_enhanced_mouse_button_event(event.button, false);
            break;

        case SDL_MOUSEMOTION:
            handle_mouse_motion_event(event.motion);
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

void CompositorThread::handle_key_event(const SDL_KeyboardEvent& key_event, bool pressed) {
    int keycode = sdl_key_to_abstract_key(key_event.keysym.sym);
    
    // Update key state (thread-safe)
    runtime_state_->key_states[keycode].store(pressed, std::memory_order_release);
    
    // Add to input queue for WAITKEY/INKEY
    if (pressed) {
        AbstractRuntime::InputEvent input_event = {
            .type = AbstractRuntime::INPUT_KEY_PRESS,
            .keycode = keycode,
            .timestamp = get_ticks()
        };
        runtime_state_->input_queue.enqueue(input_event);
    }
}

void CompositorThread::handle_mouse_button_event(const SDL_MouseButtonEvent& button_event, bool pressed) {
    int button_mask = 0;
    switch (button_event.button) {
        case SDL_BUTTON_LEFT:   button_mask = MOUSE_LEFT; break;
        case SDL_BUTTON_RIGHT:  button_mask = MOUSE_RIGHT; break;
        case SDL_BUTTON_MIDDLE: button_mask = MOUSE_MIDDLE; break;
    }
    
    // Update mouse button state
    uint32_t current_buttons = runtime_state_->mouse_buttons.load(std::memory_order_acquire);
    uint32_t new_buttons = pressed ? (current_buttons | button_mask) : (current_buttons & ~button_mask);
    runtime_state_->mouse_buttons.store(new_buttons, std::memory_order_release);
    
    // Track button clicks for mouse_clicked() function
    if (pressed) {
        runtime_state_->mouse_clicked_flags.fetch_or(button_mask, std::memory_order_release);
    }
}

void CompositorThread::handle_mouse_motion_event(const SDL_MouseMotionEvent& motion_event) {
    runtime_state_->mouse_x.store(motion_event.x, std::memory_order_release);
    runtime_state_->mouse_y.store(motion_event.y, std::memory_order_release);
}

void CompositorThread::handle_window_event(const SDL_WindowEvent& window_event) {
    switch (window_event.event) {
        case SDL_WINDOWEVENT_RESIZED:
            // Update screen dimensions and viewport
            screen_width_ = window_event.data1;
            screen_height_ = window_event.data2;
            glViewport(0, 0, screen_width_, screen_height_);
            
            // Reinitialize layer renderer with new dimensions
            if (layer_renderer_) {
                layer_renderer_->resize(screen_width_, screen_height_);
            }
            
            // Mark all layers as dirty to trigger re-render
            for (int i = 0; i < NUM_LAYERS; ++i) {
                runtime_state_->layers[i].dirty = true;
            }
            
            std::cout << "Render thread: Window resized to " << screen_width_ << "x" << screen_height_ << std::endl;
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
            // Could pause rendering when minimized for efficiency
            break;

        case SDL_WINDOWEVENT_RESTORED:
            // Resume normal rendering
            break;

        default:
            // Other window events don't need special handling
            break;
    }
}

int CompositorThread::sdl_key_to_abstract_key(SDL_Keycode sdl_key) {
    switch (sdl_key) {
        case SDLK_SPACE: return KEYCODE_SPACE;
        case SDLK_RETURN: return KEYCODE_ENTER;
        case SDLK_ESCAPE: return KEYCODE_ESC;
        case SDLK_BACKSPACE: return KEYCODE_BACKSPACE;
        case SDLK_DELETE: return KEYCODE_DELETE;
        case SDLK_LEFT: return KEYCODE_LEFT;
        case SDLK_RIGHT: return KEYCODE_RIGHT;
        case SDLK_UP: return KEYCODE_UP;
        case SDLK_DOWN: return KEYCODE_DOWN;
        case SDLK_HOME: return KEYCODE_HOME;
        case SDLK_END: return KEYCODE_END;
        default:
            // For regular characters, return the ASCII value
            if (sdl_key >= 32 && sdl_key <= 126) {
                return sdl_key;
            }
            return 0;  // Unknown key
    }
}

void CompositorThread::get_screen_dimensions(int screen_mode, int& width, int& height) {
    switch (screen_mode) {
        case SCREEN_20_COLUMN:
            width = 400; height = 480;
            break;
        case SCREEN_40_COLUMN:
            width = 640; height = 480;
            break;
        case SCREEN_80_COLUMN:
            width = 1200; height = 480;
            break;
        default:
            width = 640; height = 480;  // Default fallback
            break;
    }
}

void CompositorThread::initialize_layers() {
    std::cout << "DEBUG: initialize_layers() called" << std::endl;
    // Initialize each layer with appropriate settings
    for (int i = 0; i < NUM_LAYERS; ++i) {
        Layer& layer = runtime_state_->layers[i];
        layer.type = static_cast<LayerType>(i);
        layer.visible = true;
        layer.dirty = true;  // Mark all layers dirty initially for first render
        layer.viewport_offset = {0, 0};
        
        std::cout << "DEBUG: Initialized layer " << i << " type=" << layer.type << " dirty=" << layer.dirty << std::endl;
        
        // Initialize layer-specific data
        switch (layer.type) {
            case LAYER_TEXT:
                initialize_text_layer(layer);
                break;
            case LAYER_TILES1:
            case LAYER_TILES2:
                initialize_tile_layer(layer);
                break;
            default:
                // Other layers initialize as needed
                break;
        }
    }
}

void CompositorThread::initialize_text_layer(Layer& layer) {
    // Calculate text grid dimensions (1.5x screen size for scrolling)
    int text_cols = (screen_width_ * 3) / (2 * CHAR_WIDTH);
    int text_rows = (screen_height_ * 3) / (2 * CHAR_HEIGHT);
    
    layer.text_data.cols = text_cols;
    layer.text_data.rows = text_rows;
    layer.text_data.cursor_x = 0;
    layer.text_data.cursor_y = 0;
    
    // Initialize text buffer
    int buffer_size = text_cols * text_rows;
    layer.text_data.characters.resize(buffer_size, ' ');
    layer.text_data.colors.resize(buffer_size, COLOR_WHITE);
}

void CompositorThread::initialize_tile_layer(Layer& layer) {
    // Calculate tile grid dimensions (2x screen size for scrolling)
    int tile_cols = (screen_width_ * 2) / TILE_SIZE;
    int tile_rows = (screen_height_ * 2) / TILE_SIZE;
    
    layer.tile_data.cols = tile_cols;
    layer.tile_data.rows = tile_rows;
    
    // Initialize tile buffer (empty tiles = -1)
    int buffer_size = tile_cols * tile_rows;
    layer.tile_data.tiles.resize(buffer_size, -1);
}

void CompositorThread::update_performance_stats(int frame_time_ms) {
    total_frame_time_ += frame_time_ms;
    
    // Update stats every second
    if (frame_count_ % target_fps_ == 0) {
        float avg_frame_time = static_cast<float>(total_frame_time_) / target_fps_;
        float actual_fps = 1000.0f / avg_frame_time;
        
        // Store performance data
        runtime_state_->performance_stats.fps = actual_fps;
        runtime_state_->performance_stats.avg_frame_time_ms = avg_frame_time;
        runtime_state_->performance_stats.frame_count = frame_count_;
        
        // Reset counters
        total_frame_time_ = 0;
        
        // Optional: log performance in debug mode
        #ifdef DEBUG_PERFORMANCE
        std::cout << "FPS: " << actual_fps << ", Frame time: " << avg_frame_time << "ms" << std::endl;
        #endif
    }
}

void CompositorThread::render_fps_overlay() {
    // Get current FPS from performance stats
    float fps = runtime_state_->performance_stats.fps;
    float frame_time = runtime_state_->performance_stats.avg_frame_time_ms;
    
    // Create FPS text string
    char fps_text[64];
    snprintf(fps_text, sizeof(fps_text), "FPS: %.1f (%.1fms)", fps, frame_time);
    
    // Check if we have font atlas available for proper text rendering
    if (runtime_state_->font_atlas && runtime_state_->font_atlas.get()) {
        render_fps_overlay_with_font(fps_text);
    } else {
        render_fps_overlay_simple(fps_text);
    }
}

void CompositorThread::render_fps_overlay_with_font(const char* fps_text) {
    // Set up orthographic projection for overlay rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Get font metrics
    auto* font_atlas = runtime_state_->font_atlas.get();
    int char_width = 8;  // Default fallback
    int char_height = 16; // Default fallback
    
    // Try to get actual character metrics from font atlas
    auto char_metrics = font_atlas->get_character('A');
    if (char_metrics) {
        char_width = char_metrics->advance;
        char_height = char_metrics->height;
    }
    
    // Position in top-right corner with padding
    int text_len = strlen(fps_text);
    int padding = 10;
    int start_x = screen_width_ - (text_len * char_width) - padding;
    int start_y = padding;
    
    // Draw semi-transparent background behind text for readability
    int bg_padding = 4;
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);  // Semi-transparent black
    glBegin(GL_QUADS);
        glVertex2i(start_x - bg_padding, start_y - bg_padding);
        glVertex2i(screen_width_ - padding + bg_padding, start_y - bg_padding);
        glVertex2i(screen_width_ - padding + bg_padding, start_y + char_height + bg_padding);
        glVertex2i(start_x - bg_padding, start_y + char_height + bg_padding);
    glEnd();
    
    glDisable(GL_BLEND);
    
    // Restore previous matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void CompositorThread::render_fps_overlay_simple(const char* fps_text) {
    // Set up orthographic projection for overlay rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable depth test for overlay
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Use fixed character dimensions for simple overlay
    int char_width = 8;
    int char_height = 16;
    
    // Position in top-right corner with padding
    int text_len = strlen(fps_text);
    int padding = 10;
    int start_x = screen_width_ - (text_len * char_width) - padding;
    int start_y = padding;
    
    // Draw semi-transparent background behind text for readability
    int bg_padding = 4;
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);  // Semi-transparent black
    glBegin(GL_QUADS);
        glVertex2i(start_x - bg_padding, start_y - bg_padding);
        glVertex2i(screen_width_ - padding + bg_padding, start_y - bg_padding);
        glVertex2i(screen_width_ - padding + bg_padding, start_y + char_height + bg_padding);
        glVertex2i(start_x - bg_padding, start_y + char_height + bg_padding);
    glEnd();
    
    // Draw text in white (simple bitmap rendering would go here)
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);  // White text
    // Note: This is a placeholder - proper bitmap font rendering would be implemented here
    
    glDisable(GL_BLEND);
    
    // Restore previous matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);
}

void CompositorThread::cleanup() {
    std::cout << "Compositor cleanup: Releasing layer renderer..." << std::endl;
    layer_renderer_.reset();
    
    // Only clean up GL context if we own it
    if (owns_context_ && gl_context_) {
        std::cout << "Compositor cleanup: Deleting GL context..." << std::endl;
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    
    // Only destroy window if we own it
    if (owns_context_ && sdl_window_) {
        std::cout << "Compositor cleanup: Destroying SDL window..." << std::endl;
        SDL_DestroyWindow(sdl_window_);
        sdl_window_ = nullptr;
    }
    
    // Only quit SDL if we initialized it
    if (owns_context_) {
        std::cout << "Compositor cleanup: Quitting SDL..." << std::endl;
        SDL_Quit();
    }
    
    std::cout << "Compositor cleanup complete" << std::endl;
}

uint64_t CompositorThread::get_ticks() {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void CompositorThread::bring_window_to_front() {
    if (sdl_window_) {
        SDL_RaiseWindow(sdl_window_);
        SDL_SetWindowInputFocus(sdl_window_);
    }
}

} // namespace AbstractRuntime