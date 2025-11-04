#include "sprite_allocator.h"
#include <algorithm>
#include <iostream>

namespace AbstractRuntime {

// Static instance management
SpriteAllocator& SpriteAllocator::getInstance() {
    static SpriteAllocator instance;
    return instance;
}

void SpriteAllocator::initializeAvailableSprites() {
    if (initialized_) return;
    
    for (SpriteID id = MIN_SPRITE_ID; id <= MAX_SPRITE_ID; ++id) {
        available_sprites_.insert(id);
    }
    initialized_ = true;
}

SpriteAllocator::SpriteID SpriteAllocator::allocateSprite(ThreadID thread_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        initializeAvailableSprites();
    }
    
    if (available_sprites_.empty()) {
        return INVALID_SPRITE_ID;
    }
    
    // Get the first available sprite
    SpriteID sprite_id = *available_sprites_.begin();
    
    // Remove from available set
    available_sprites_.erase(sprite_id);
    
    // Record ownership
    sprite_owners_[sprite_id] = thread_id;
    thread_sprites_[thread_id].insert(sprite_id);
    
    return sprite_id;
}

std::vector<SpriteAllocator::SpriteID> SpriteAllocator::allocateSprites(ThreadID thread_id, uint32_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        initializeAvailableSprites();
    }
    
    std::vector<SpriteID> allocated;
    allocated.reserve(std::min(count, static_cast<uint32_t>(available_sprites_.size())));
    
    auto it = available_sprites_.begin();
    while (it != available_sprites_.end() && allocated.size() < count) {
        SpriteID sprite_id = *it;
        
        // Record ownership
        sprite_owners_[sprite_id] = thread_id;
        thread_sprites_[thread_id].insert(sprite_id);
        allocated.push_back(sprite_id);
        
        // Remove from available (iterator becomes invalid, so we restart)
        available_sprites_.erase(it);
        it = available_sprites_.begin();
    }
    
    return allocated;
}

bool SpriteAllocator::allocateSpecificSprite(ThreadID thread_id, SpriteID sprite_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        initializeAvailableSprites();
    }
    
    // Check if sprite ID is valid
    if (sprite_id < MIN_SPRITE_ID || sprite_id > MAX_SPRITE_ID) {
        return false;
    }
    
    // Check if sprite is available
    if (available_sprites_.find(sprite_id) == available_sprites_.end()) {
        return false; // Already allocated
    }
    
    // Remove from available set
    available_sprites_.erase(sprite_id);
    
    // Record ownership
    sprite_owners_[sprite_id] = thread_id;
    thread_sprites_[thread_id].insert(sprite_id);
    
    return true;
}

bool SpriteAllocator::releaseSprite(SpriteID sprite_id, ThreadID thread_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check ownership
    auto owner_it = sprite_owners_.find(sprite_id);
    if (owner_it == sprite_owners_.end() || owner_it->second != thread_id) {
        return false; // Not owned by this thread
    }
    
    // Remove ownership records
    sprite_owners_.erase(owner_it);
    
    auto thread_it = thread_sprites_.find(thread_id);
    if (thread_it != thread_sprites_.end()) {
        thread_it->second.erase(sprite_id);
        if (thread_it->second.empty()) {
            thread_sprites_.erase(thread_it);
        }
    }
    
    // Return to available pool
    available_sprites_.insert(sprite_id);
    
    return true;
}

uint32_t SpriteAllocator::releaseAllSprites(ThreadID thread_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto thread_it = thread_sprites_.find(thread_id);
    if (thread_it == thread_sprites_.end()) {
        return 0;
    }
    
    uint32_t released_count = 0;
    for (SpriteID sprite_id : thread_it->second) {
        // Remove from ownership map
        sprite_owners_.erase(sprite_id);
        
        // Return to available pool
        available_sprites_.insert(sprite_id);
        
        ++released_count;
    }
    
    // Remove thread from tracking
    thread_sprites_.erase(thread_it);
    
    return released_count;
}

bool SpriteAllocator::isOwnedByThread(SpriteID sprite_id, ThreadID thread_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sprite_owners_.find(sprite_id);
    return it != sprite_owners_.end() && it->second == thread_id;
}

bool SpriteAllocator::isSpriteAvailable(SpriteID sprite_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sprite_id < MIN_SPRITE_ID || sprite_id > MAX_SPRITE_ID) {
        return false;
    }
    
    return available_sprites_.find(sprite_id) != available_sprites_.end();
}

SpriteAllocator::ThreadID SpriteAllocator::getSpriteOwner(SpriteID sprite_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sprite_owners_.find(sprite_id);
    return it != sprite_owners_.end() ? it->second : 0;
}

std::vector<SpriteAllocator::SpriteID> SpriteAllocator::getSpritesOwnedBy(ThreadID thread_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = thread_sprites_.find(thread_id);
    if (it == thread_sprites_.end()) {
        return {};
    }
    
    return std::vector<SpriteID>(it->second.begin(), it->second.end());
}

uint32_t SpriteAllocator::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(available_sprites_.size());
}

uint32_t SpriteAllocator::getAllocatedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<uint32_t>(sprite_owners_.size());
}

void SpriteAllocator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sprite_owners_.clear();
    thread_sprites_.clear();
    available_sprites_.clear();
    
    // Reinitialize
    for (SpriteID id = MIN_SPRITE_ID; id <= MAX_SPRITE_ID; ++id) {
        available_sprites_.insert(id);
    }
}

SpriteAllocator::Stats SpriteAllocator::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total_sprites = MAX_SPRITE_ID - MIN_SPRITE_ID + 1;
    stats.allocated_sprites = static_cast<uint32_t>(sprite_owners_.size());
    stats.available_sprites = static_cast<uint32_t>(available_sprites_.size());
    stats.active_threads = static_cast<uint32_t>(thread_sprites_.size());
    
    return stats;
}

// RAII Guard Implementation

SpriteOwnershipGuard::SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id)
    : thread_id_(thread_id) {
    auto& allocator = SpriteAllocator::getInstance();
    SpriteAllocator::SpriteID sprite_id = allocator.allocateSprite(thread_id_);
    if (sprite_id != SpriteAllocator::INVALID_SPRITE_ID) {
        owned_sprites_.push_back(sprite_id);
    }
}

SpriteOwnershipGuard::SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id, uint32_t count)
    : thread_id_(thread_id) {
    auto& allocator = SpriteAllocator::getInstance();
    owned_sprites_ = allocator.allocateSprites(thread_id_, count);
}

SpriteOwnershipGuard::SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id, SpecificSpriteID specific_id)
    : thread_id_(thread_id) {
    auto& allocator = SpriteAllocator::getInstance();
    if (allocator.allocateSpecificSprite(thread_id_, specific_id.id)) {
        owned_sprites_.push_back(specific_id.id);
    }
}

SpriteOwnershipGuard::~SpriteOwnershipGuard() {
    auto& allocator = SpriteAllocator::getInstance();
    for (SpriteAllocator::SpriteID sprite_id : owned_sprites_) {
        allocator.releaseSprite(sprite_id, thread_id_);
    }
}

SpriteOwnershipGuard::SpriteOwnershipGuard(SpriteOwnershipGuard&& other) noexcept
    : thread_id_(other.thread_id_), owned_sprites_(std::move(other.owned_sprites_)) {
    other.owned_sprites_.clear();
}

SpriteOwnershipGuard& SpriteOwnershipGuard::operator=(SpriteOwnershipGuard&& other) noexcept {
    if (this != &other) {
        // Release current sprites
        auto& allocator = SpriteAllocator::getInstance();
        for (SpriteAllocator::SpriteID sprite_id : owned_sprites_) {
            allocator.releaseSprite(sprite_id, thread_id_);
        }
        
        // Take ownership from other
        thread_id_ = other.thread_id_;
        owned_sprites_ = std::move(other.owned_sprites_);
        other.owned_sprites_.clear();
    }
    return *this;
}

} // namespace AbstractRuntime