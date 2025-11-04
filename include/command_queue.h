/*
 * CommandQueue - Thread-safe command passing system
 * 
 * Used in current architecture for:
 * - Background application thread sending UI commands to main thread
 * - Ensuring thread-safe communication between app and renderer
 * 
 * Current architecture (macOS compatible):
 * - Main thread runs the runtime/renderer (required for macOS UI events)
 * - Background thread runs the application
 * - Application uses this queue to send UI commands safely to main thread
 * 
 * Note: Currently print_at(), clear_text() etc. write directly to buffers
 * which is NOT thread-safe. These should be converted to use this queue.
 */

#pragma once

#include <SDL2/SDL.h>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace AbstractRuntime {

/**
 * Thread-safe command queue for passing events from main thread to render thread.
 * Uses a simple mutex-protected queue with efficient drain operation.
 */
class CommandQueue {
public:
    CommandQueue() = default;
    ~CommandQueue() = default;

    // Non-copyable
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;

    /**
     * Push an event onto the queue (called by main thread)
     * @param event SDL event to queue
     */
    void push_event(const SDL_Event& event);

    /**
     * Try to pop a single event from the queue (called by render thread)
     * @param event Output parameter for the event
     * @return true if an event was popped, false if queue was empty
     */
    bool try_pop_event(SDL_Event& event);

    /**
     * Drain all events from the queue into a local buffer (more efficient)
     * @param events Output vector to receive all queued events
     * @return number of events drained
     */
    size_t drain_events(std::vector<SDL_Event>& events);

    /**
     * Check if the queue is empty
     * @return true if queue is empty
     */
    bool empty() const;

    /**
     * Get current queue size (approximate, for debugging)
     * @return number of events in queue
     */
    size_t size() const;

    /**
     * Clear all events from the queue
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::queue<SDL_Event> event_queue_;
};

} // namespace AbstractRuntime