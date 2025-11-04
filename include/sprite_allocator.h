#pragma once

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cstdint>

namespace AbstractRuntime {

/**
 * @brief Sprite Ownership Allocator
 * 
 * Manages sprite ID allocation to prevent conflicts between multiple Lua threads.
 * Each thread can be assigned exclusive ownership of specific sprite IDs.
 * 
 * Thread-safe implementation allows concurrent allocation and deallocation.
 */
class SpriteAllocator {
public:
    using SpriteID = uint32_t;
    using ThreadID = uint64_t;
    
    static constexpr SpriteID MIN_SPRITE_ID = 1;
    static constexpr SpriteID MAX_SPRITE_ID = 128;
    static constexpr SpriteID INVALID_SPRITE_ID = 0;
    
    /**
     * @brief Get the global sprite allocator instance
     */
    static SpriteAllocator& getInstance();
    
    /**
     * @brief Allocate a sprite ID for exclusive use by a thread
     * @param thread_id The thread requesting the sprite
     * @return Allocated sprite ID, or INVALID_SPRITE_ID if none available
     */
    SpriteID allocateSprite(ThreadID thread_id);
    
    /**
     * @brief Allocate multiple sprite IDs for a thread
     * @param thread_id The thread requesting the sprites
     * @param count Number of sprites to allocate
     * @return Vector of allocated sprite IDs (may be fewer than requested)
     */
    std::vector<SpriteID> allocateSprites(ThreadID thread_id, uint32_t count);
    
    /**
     * @brief Allocate a specific sprite ID if available
     * @param thread_id The thread requesting the sprite
     * @param sprite_id The specific sprite ID to allocate
     * @return true if successfully allocated, false if already in use
     */
    bool allocateSpecificSprite(ThreadID thread_id, SpriteID sprite_id);
    
    /**
     * @brief Release a sprite ID back to the pool
     * @param sprite_id The sprite ID to release
     * @param thread_id The thread that owns the sprite (for verification)
     * @return true if successfully released, false if not owned by thread
     */
    bool releaseSprite(SpriteID sprite_id, ThreadID thread_id);
    
    /**
     * @brief Release all sprites owned by a thread
     * @param thread_id The thread whose sprites should be released
     * @return Number of sprites released
     */
    uint32_t releaseAllSprites(ThreadID thread_id);
    
    /**
     * @brief Check if a sprite is owned by a specific thread
     * @param sprite_id The sprite ID to check
     * @param thread_id The thread to check ownership for
     * @return true if the thread owns the sprite
     */
    bool isOwnedByThread(SpriteID sprite_id, ThreadID thread_id) const;
    
    /**
     * @brief Check if a sprite is available for allocation
     * @param sprite_id The sprite ID to check
     * @return true if available, false if already allocated
     */
    bool isSpriteAvailable(SpriteID sprite_id) const;
    
    /**
     * @brief Get the owner of a sprite
     * @param sprite_id The sprite ID to query
     * @return Thread ID of owner, or 0 if unallocated
     */
    ThreadID getSpriteOwner(SpriteID sprite_id) const;
    
    /**
     * @brief Get all sprites owned by a thread
     * @param thread_id The thread to query
     * @return Vector of sprite IDs owned by the thread
     */
    std::vector<SpriteID> getSpritesOwnedBy(ThreadID thread_id) const;
    
    /**
     * @brief Get number of available sprites
     * @return Number of unallocated sprites
     */
    uint32_t getAvailableCount() const;
    
    /**
     * @brief Get number of allocated sprites
     * @return Number of allocated sprites
     */
    uint32_t getAllocatedCount() const;
    
    /**
     * @brief Reset allocator (release all sprites)
     * WARNING: This should only be called during shutdown
     */
    void reset();
    
    /**
     * @brief Get allocation statistics
     */
    struct Stats {
        uint32_t total_sprites;
        uint32_t allocated_sprites;
        uint32_t available_sprites;
        uint32_t active_threads;
    };
    
    Stats getStats() const;
    
private:
    SpriteAllocator() = default;
    ~SpriteAllocator() = default;
    
    // Non-copyable, non-movable
    SpriteAllocator(const SpriteAllocator&) = delete;
    SpriteAllocator& operator=(const SpriteAllocator&) = delete;
    SpriteAllocator(SpriteAllocator&&) = delete;
    SpriteAllocator& operator=(SpriteAllocator&&) = delete;
    
    mutable std::mutex mutex_;
    
    // Map sprite ID to owning thread ID (0 = unallocated)
    std::unordered_map<SpriteID, ThreadID> sprite_owners_;
    
    // Set of available sprite IDs for quick allocation
    std::unordered_set<SpriteID> available_sprites_;
    
    // Map thread ID to set of owned sprite IDs
    std::unordered_map<ThreadID, std::unordered_set<SpriteID>> thread_sprites_;
    
    /**
     * @brief Initialize available sprites set (called once)
     */
    void initializeAvailableSprites();
    
    /**
     * @brief Check if allocator is initialized
     */
    bool initialized_ = false;
};

/**
 * @brief Specific sprite ID wrapper to avoid constructor ambiguity
 */
struct SpecificSpriteID {
    SpriteAllocator::SpriteID id;
    explicit SpecificSpriteID(SpriteAllocator::SpriteID sprite_id) : id(sprite_id) {}
};

/**
 * @brief RAII Sprite Ownership Guard
 * 
 * Automatically allocates sprite(s) on construction and releases on destruction.
 * Useful for temporary sprite usage in functions.
 */
class SpriteOwnershipGuard {
public:
    /**
     * @brief Allocate a single sprite for the thread
     */
    explicit SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id);
    
    /**
     * @brief Allocate multiple sprites for the thread
     */
    SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id, uint32_t count);
    
    /**
     * @brief Allocate a specific sprite for the thread
     */
    SpriteOwnershipGuard(SpriteAllocator::ThreadID thread_id, SpecificSpriteID specific_id);
    
    /**
     * @brief Release all owned sprites
     */
    ~SpriteOwnershipGuard();
    
    // Non-copyable, but movable
    SpriteOwnershipGuard(const SpriteOwnershipGuard&) = delete;
    SpriteOwnershipGuard& operator=(const SpriteOwnershipGuard&) = delete;
    
    SpriteOwnershipGuard(SpriteOwnershipGuard&& other) noexcept;
    SpriteOwnershipGuard& operator=(SpriteOwnershipGuard&& other) noexcept;
    
    /**
     * @brief Get the allocated sprite IDs
     */
    const std::vector<SpriteAllocator::SpriteID>& getSprites() const { return owned_sprites_; }
    
    /**
     * @brief Get the first allocated sprite ID (for single sprite allocation)
     */
    SpriteAllocator::SpriteID getSprite() const {
        return owned_sprites_.empty() ? SpriteAllocator::INVALID_SPRITE_ID : owned_sprites_[0];
    }
    
    /**
     * @brief Check if allocation was successful
     */
    bool isValid() const { return !owned_sprites_.empty(); }
    
    /**
     * @brief Get number of successfully allocated sprites
     */
    size_t getCount() const { return owned_sprites_.size(); }
    
private:
    SpriteAllocator::ThreadID thread_id_;
    std::vector<SpriteAllocator::SpriteID> owned_sprites_;
};

} // namespace AbstractRuntime