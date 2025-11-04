/*
 * UNUSED CODE - OLD ARCHITECTURE
 * 
 * This CompositorThread header was designed for an architecture where:
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
 */

#ifndef COMPOSITOR_THREAD_H
#define COMPOSITOR_THREAD_H

#include <SDL2/SDL.h>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>
#include <vector>

// Forward declarations
namespace AbstractRuntime {
    struct RuntimeState;
    class LayerRenderer;
    struct Layer;
    class CommandQueue;
}

namespace AbstractRuntime {

/**
 * CompositorThread manages the 60fps rendering loop and handles all GPU operations.
 * It runs independently of user code to guarantee smooth frame rates.
 */
class CompositorThread {
public:
    explicit CompositorThread(AbstractRuntime::RuntimeState* state);
    ~CompositorThread();

    /**
     * Initialize the compositor with SDL/OpenGL context
     * @param screen_mode Screen resolution mode (0=640x480, 1=800x600, 2=1920x1080)
     * @return true on success, false on failure
     */
    bool initialize(int screen_mode);

    /**
     * Initialize compositor with existing window and context (new architecture)
     * @param window Existing SDL window created by main thread
     * @param context Existing OpenGL context created by main thread  
     * @param width Window width
     * @param height Window height
     * @return true on success, false on failure
     */
    bool initialize_with_existing_context(SDL_Window* window, SDL_GLContext context, int width, int height);

    /**
     * Set the command queue for receiving events from main thread
     * @param queue Command queue pointer (managed by MainThreadManager)
     */
    void set_command_queue(CommandQueue* queue);

    /**
     * Request the compositor thread to quit
     */
    void request_quit();

    /**
     * Start the compositor thread
     */
    void start();

    /**
     * Stop the compositor thread and cleanup resources
     */
    void stop();

    /**
     * Check if compositor is currently running
     * @return true if running
     */
    bool is_running() const { return running_; }

    /**
     * Bring window to front and focus it
     */
    void bring_window_to_front();

    /**
     * Get current screen dimensions
     */
    int get_screen_width() const { return screen_width_; }
    int get_screen_height() const { return screen_height_; }

private:
    // Core rendering loop
    void render_loop();
    void render_frame();
    void render_layer(int layer_id);

    // Layer-specific rendering
    void render_background_layer(const AbstractRuntime::Layer& layer);
    void render_tile_layer(const AbstractRuntime::Layer& layer);
    void render_graphics_layer(const AbstractRuntime::Layer& layer);
    void render_sprite_layer(const AbstractRuntime::Layer& layer);
    void render_text_layer(const AbstractRuntime::Layer& layer);

    // Utility functions
    bool check_dirty_layers();
    void initialize_layers();
    void initialize_text_layer(AbstractRuntime::Layer& layer);
    void initialize_tile_layer(AbstractRuntime::Layer& layer);

    // Event handling (new architecture)
    void drain_and_process_events();
    void process_event(const SDL_Event& event);
    void handle_key_event(const SDL_KeyboardEvent& key_event, bool pressed);
    void handle_mouse_button_event(const SDL_MouseButtonEvent& button_event, bool pressed);
    void handle_mouse_motion_event(const SDL_MouseMotionEvent& motion_event);
    void handle_window_event(const SDL_WindowEvent& window_event);

    // Enhanced input event handlers
    void handle_enhanced_key_event(const SDL_KeyboardEvent& key_event, bool pressed);
    void handle_enhanced_mouse_button_event(const SDL_MouseButtonEvent& button_event, bool pressed);
    void handle_enhanced_mouse_motion_event(const SDL_MouseMotionEvent& motion_event);
    void handle_enhanced_mouse_wheel_event(const SDL_MouseWheelEvent& wheel_event);
    void process_enhanced_event(const SDL_Event& event);
    void update_enhanced_input_frame();
    void initialize_enhanced_input();

    // Input conversion
    int sdl_key_to_abstract_key(SDL_Keycode sdl_key);

    // Screen mode utilities
    void get_screen_dimensions(int screen_mode, int& width, int& height);

    // Performance monitoring
    void update_performance_stats(int frame_time_ms);
    
    // FPS overlay rendering
    void render_fps_overlay();
    void render_fps_overlay_with_font(const char* fps_text);
    void render_fps_overlay_simple(const char* fps_text);

    // Cleanup
    void cleanup();

    // Timing utilities
    uint64_t get_ticks();

    // Member variables
    AbstractRuntime::RuntimeState* runtime_state_;
    std::thread compositor_thread_;
    std::atomic<bool> running_;

    // Rendering settings
    int target_fps_;
    float frame_time_ms_;
    int screen_width_;
    int screen_height_;

    // SDL/OpenGL resources
    SDL_Window* sdl_window_;
    SDL_GLContext gl_context_;
    std::unique_ptr<AbstractRuntime::LayerRenderer> layer_renderer_;
    bool owns_context_;  // Whether we created the context or received it from main thread

    // Event processing
    CommandQueue* command_queue_;  // Not owned - managed by MainThreadManager
    std::vector<SDL_Event> frame_events_;  // Temporary buffer for draining events

    // Performance tracking
    uint64_t frame_count_ = 0;
    uint64_t total_frame_time_ = 0;

    // Constants
    static constexpr int SPRITE_SIZE = 64;
    static constexpr int TILE_SIZE = 64;
    static constexpr int CHAR_WIDTH = 8;
    static constexpr int CHAR_HEIGHT = 16;
    static constexpr int MAX_SPRITES = 128;
    static constexpr int NUM_LAYERS = 6;
};

} // namespace AbstractRuntime

#endif // COMPOSITOR_THREAD_H