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

#include "command_queue.h"
#include <algorithm>

namespace AbstractRuntime {

void CommandQueue::push_event(const SDL_Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_queue_.push(event);
}

bool CommandQueue::try_pop_event(SDL_Event& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (event_queue_.empty()) {
        return false;
    }
    
    event = event_queue_.front();
    event_queue_.pop();
    return true;
}

size_t CommandQueue::drain_events(std::vector<SDL_Event>& events) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reserve space for efficiency
    events.clear();
    events.reserve(event_queue_.size());
    
    // Move all events to the output vector
    size_t count = 0;
    while (!event_queue_.empty()) {
        events.push_back(event_queue_.front());
        event_queue_.pop();
        count++;
    }
    
    return count;
}

bool CommandQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return event_queue_.empty();
}

size_t CommandQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return event_queue_.size();
}

void CommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Clear the queue by creating a new empty one
    std::queue<SDL_Event> empty_queue;
    event_queue_.swap(empty_queue);
}

} // namespace AbstractRuntime