/*
 * UNUSED CODE - OLD ARCHITECTURE
 * 
 * This MainThreadManager header was designed for an architecture where:
 * - Main thread ran the application
 * - Background thread ran the compositor/renderer
 * - Main thread forwarded events to background renderer via command queue
 * 
 * Current architecture (macOS compatible):
 * - Main thread runs the runtime/renderer (required for macOS UI events)
 * - Background thread runs the application
 * - Events processed directly on main thread in abstract_runtime.cpp
 * 
 * This file is kept for reference but is not used in the current system.
 */

#pragma once

#include <SDL2/SDL.h>
#include <atomic>
#include <thread>
#include <memory>

namespace AbstractRuntime {

class CommandQueue;
class CompositorThread;
struct RuntimeState;

/**
 * MainThreadManager handles SDL initialization and event processing on the main thread.
 * This is required on macOS where SDL/Cocoa events must be processed on the main thread.
 * 
 * Architecture:
 * - Main thread: Initialize SDL, create window, run event loop with SDL_WaitEvent
 * - Render thread: Run 60fps loop, drain command queue, render frames
 * - Communication: Thread-safe command queue passes events from main to render thread
 */
class MainThreadManager {
public:
    MainThreadManager();
    ~MainThreadManager();

    // Non-copyable
    MainThreadManager(const MainThreadManager&) = delete;
    MainThreadManager& operator=(const MainThreadManager&) = delete;

    /**
     * Initialize SDL and create the window/renderer on the main thread.
     * Must be called from the main thread.
     * @param screen_mode Screen resolution mode
     * @param runtime_state Shared runtime state
     * @return true if initialization succeeded
     */
    bool initialize(int screen_mode, RuntimeState* runtime_state);

    /**
     * Start the render thread and begin the main event loop.
     * This will block until the application should quit.
     * Must be called from the main thread.
     * @return exit code (0 for normal exit)
     */
    int run();

    /**
     * Request shutdown of the application.
     * Can be called from any thread.
     */
    void request_quit();

    /**
     * Check if the application should quit.
     * @return true if quit was requested
     */
    bool should_quit() const;

    /**
     * Get the SDL window (for render thread initialization).
     * @return SDL window pointer
     */
    SDL_Window* get_window() const { return sdl_window_; }

    /**
     * Get the SDL renderer (for render thread initialization).
     * @return SDL renderer pointer
     */
    SDL_Renderer* get_renderer() const { return sdl_renderer_; }

private:
    /**
     * Main event loop - processes events using SDL_WaitEvent and forwards to command queue.
     * Runs on the main thread.
     */
    void event_loop();

    /**
     * Process a single SDL event and forward it to the render thread if needed.
     * @param event SDL event to process
     */
    void process_event(const SDL_Event& event);

    /**
     * Handle window events that need immediate main thread processing.
     * @param event Window event
     */
    void handle_window_event(const SDL_WindowEvent& event);

    /**
     * Get screen dimensions for the given screen mode.
     * @param screen_mode Screen mode constant
     * @param width Output width
     * @param height Output height
     */
    void get_screen_dimensions(int screen_mode, int& width, int& height);

    /**
     * Cleanup SDL resources.
     */
    void cleanup();

    // SDL resources (owned by main thread)
    SDL_Window* sdl_window_;
    SDL_Renderer* sdl_renderer_;
    SDL_GLContext gl_context_;

    // Screen dimensions
    int screen_width_;
    int screen_height_;

    // Shared state
    RuntimeState* runtime_state_;
    std::unique_ptr<CommandQueue> command_queue_;
    std::unique_ptr<CompositorThread> compositor_thread_;

    // Control flags
    std::atomic<bool> quit_requested_;
    std::atomic<bool> initialized_;

    // Performance tracking
    uint64_t frame_count_;
    uint64_t event_count_;
};

} // namespace AbstractRuntime