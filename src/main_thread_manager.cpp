/*
 * UNUSED CODE - OLD ARCHITECTURE
 * 
 * This MainThreadManager was designed for an architecture where:
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

#include "main_thread_manager.h"
#include "command_queue.h"
#include "compositor_thread.h"
#include "runtime_state.h"
#include <iostream>
#include <chrono>

namespace AbstractRuntime {

MainThreadManager::MainThreadManager()
    : sdl_window_(nullptr)
    , sdl_renderer_(nullptr)
    , gl_context_(nullptr)
    , screen_width_(640)
    , screen_height_(480)
    , runtime_state_(nullptr)
    , quit_requested_(false)
    , initialized_(false)
    , frame_count_(0)
    , event_count_(0) {
}

MainThreadManager::~MainThreadManager() {
    cleanup();
}

bool MainThreadManager::initialize(int screen_mode, RuntimeState* runtime_state) {
    if (initialized_.load()) {
        std::cerr << "MainThreadManager already initialized" << std::endl;
        return false;
    }

    runtime_state_ = runtime_state;

    // Initialize SDL (must be called from main thread on macOS)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    // Get screen dimensions
    get_screen_dimensions(screen_mode, screen_width_, screen_height_);

    // Set OpenGL attributes for compatibility rendering (supports immediate mode)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window (must be done on main thread)
    sdl_window_ = SDL_CreateWindow(
        "NewBCPL Graphics Terminal",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        screen_width_, screen_height_,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!sdl_window_) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        cleanup();
        return false;
    }

    // Create OpenGL context
    gl_context_ = SDL_GL_CreateContext(sdl_window_);
    if (!gl_context_) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        cleanup();
        return false;
    }

    // Create command queue for thread communication
    command_queue_ = std::make_unique<CommandQueue>();

    // Create compositor thread (but don't start it yet)
    compositor_thread_ = std::make_unique<CompositorThread>(runtime_state_);

    // Make sure window is visible and focused on macOS
    SDL_RaiseWindow(sdl_window_);
    SDL_SetWindowInputFocus(sdl_window_);

    initialized_.store(true);
    std::cout << "MainThreadManager initialized: " << screen_width_ << "x" << screen_height_ << std::endl;
    return true;
}

int MainThreadManager::run() {
    if (!initialized_.load()) {
        std::cerr << "MainThreadManager not initialized" << std::endl;
        return 1;
    }

    // Initialize compositor with the created window and context
    if (!compositor_thread_->initialize_with_existing_context(
            sdl_window_, gl_context_, screen_width_, screen_height_)) {
        std::cerr << "Failed to initialize compositor thread" << std::endl;
        return 1;
    }

    // Pass the command queue to the compositor
    compositor_thread_->set_command_queue(command_queue_.get());

    // Start the render thread
    compositor_thread_->start();

    std::cout << "Starting main event loop..." << std::endl;

    // Run the main event loop (blocks until quit)
    event_loop();

    std::cout << "Main event loop finished. Events processed: " << event_count_ << std::endl;
    return 0;
}

void MainThreadManager::request_quit() {
    std::cout << "Quit requested" << std::endl;
    quit_requested_.store(true);
    
    // Also notify the compositor thread
    if (compositor_thread_) {
        compositor_thread_->request_quit();
    }
}

bool MainThreadManager::should_quit() const {
    return quit_requested_.load();
}

void MainThreadManager::event_loop() {
    SDL_Event event;
    
    while (!quit_requested_.load()) {
        // Use SDL_WaitEvent to block and save CPU - this is key!
        // SDL_PollEvent would spin and burn 100% CPU
        if (SDL_WaitEvent(&event)) {
            event_count_++;
            process_event(event);
        } else {
            // SDL_WaitEvent failed - this shouldn't happen often
            std::cerr << "SDL_WaitEvent failed: " << SDL_GetError() << std::endl;
            
            // Small delay to prevent tight loop if there's a persistent error
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Process any additional events that might be pending
        // (SDL_WaitEvent only waits for the first event)
        while (SDL_PollEvent(&event)) {
            event_count_++;
            process_event(event);
        }
    }
}

void MainThreadManager::process_event(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            std::cout << "Received SDL_QUIT event" << std::endl;
            request_quit();
            break;

        case SDL_WINDOWEVENT:
            // Handle window events that need immediate main thread processing
            handle_window_event(event.window);
            // Also forward to render thread for rendering adjustments
            command_queue_->push_event(event);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEWHEEL:
            // Forward input events to render thread for game logic processing
            command_queue_->push_event(event);
            break;

        default:
            // Forward other events to render thread
            command_queue_->push_event(event);
            break;
    }
}

void MainThreadManager::handle_window_event(const SDL_WindowEvent& event) {
    switch (event.event) {
        case SDL_WINDOWEVENT_CLOSE:
            std::cout << "Window close requested" << std::endl;
            request_quit();
            break;

        case SDL_WINDOWEVENT_RESIZED:
            std::cout << "Window resized to " << event.data1 << "x" << event.data2 << std::endl;
            screen_width_ = event.data1;
            screen_height_ = event.data2;
            // The render thread will handle the OpenGL viewport update
            break;

        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            std::cout << "Window state changed: " << event.event << std::endl;
            break;

        default:
            // Other window events don't need special main thread handling
            break;
    }
}

void MainThreadManager::get_screen_dimensions(int screen_mode, int& width, int& height) {
    switch (screen_mode) {
        case 0: // SCREEN_640x480
            width = 640; height = 480;
            break;
        case 1: // SCREEN_800x600
            width = 800; height = 600;
            break;
        case 2: // SCREEN_1024x768
            width = 1024; height = 768;
            break;
        case 3: // SCREEN_1920x1080
            width = 1920; height = 1080;
            break;
        default:
            width = 640; height = 480;  // Default fallback
            break;
    }
}

void MainThreadManager::cleanup() {
    std::cout << "MainThreadManager cleanup starting..." << std::endl;
    
    // Stop compositor thread first if it's still running
    if (compositor_thread_) {
        std::cout << "Stopping compositor thread during cleanup..." << std::endl;
        if (compositor_thread_->is_running()) {
            compositor_thread_->stop();
        }
        compositor_thread_.reset();
        std::cout << "Compositor thread cleaned up" << std::endl;
    }

    // Clear command queue
    if (command_queue_) {
        command_queue_->clear();
        command_queue_.reset();
        std::cout << "Command queue cleaned up" << std::endl;
    }

    // Clean up OpenGL context
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
        std::cout << "OpenGL context cleaned up" << std::endl;
    }

    // Clean up SDL window
    if (sdl_window_) {
        SDL_DestroyWindow(sdl_window_);
        sdl_window_ = nullptr;
        std::cout << "SDL window cleaned up" << std::endl;
    }

    // Quit SDL if we initialized it
    if (initialized_.load()) {
        SDL_Quit();
        initialized_.store(false);
        std::cout << "SDL quit" << std::endl;
    }
    
    std::cout << "MainThreadManager cleanup complete" << std::endl;
}

} // namespace AbstractRuntime