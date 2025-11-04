#ifndef SPRITE_BANK_H
#define SPRITE_BANK_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <queue>

// Forward declarations for OpenGL
typedef unsigned int GLuint;

namespace AbstractRuntime {

/**
 * Sprite slot information
 */
struct SpriteSlot {
    bool occupied = false;
    GLuint texture_id = 0;
    int width = 128;
    int height = 128;
    std::string source_file;
    
    // For deferred texture creation (background thread -> main thread)
    bool needs_texture_creation = false;
    std::vector<unsigned char> png_data;  // RGBA pixel data
    
    SpriteSlot() = default;
    ~SpriteSlot();
};

/**
 * SpriteBank manages the sprite texture bank (512 slots).
 * Handles loading PNG files into GPU textures.
 */
/**
 * Sprite loading request (for main thread processing)
 */
struct SpriteLoadRequest {
    int slot;
    std::string filename;
    
    SpriteLoadRequest(int s, const std::string& f) : slot(s), filename(f) {}
};

/**
 * SpriteBank manages the sprite texture bank (512 slots).
 * Handles loading PNG files into GPU textures with proper thread safety.
 */
class SpriteBank {
public:
    static constexpr int BANK_SIZE = 512;
    static constexpr int DEFAULT_SPRITE_SIZE = 128;

    SpriteBank();
    ~SpriteBank();

    /**
     * Initialize sprite bank
     * @return true on success
     */
    bool initialize();

    /**
     * Shutdown and cleanup all resources
     */
    void shutdown();

    /**
     * Load sprite from PNG file
     * @param slot Slot number (0-511)
     * @param filename PNG file path
     * @return true on success
     */
    bool load_sprite(int slot, const std::string& filename);

    /**
     * Release sprite slot
     * @param slot Slot number to release
     */
    void release_sprite(int slot);

    /**
     * Get OpenGL texture ID for slot
     * @param slot Slot number
     * @return Texture ID or 0 if invalid
     */
    GLuint get_texture(int slot) const;

    /**
     * Get texture pixel data for slot
     * @param slot Slot number
     * @param data Output vector for RGBA pixel data
     * @param width Output width
     * @param height Output height
     * @return true if successful
     */
    bool get_texture_data(int slot, std::vector<unsigned char>& data, int& width, int& height) const;

    /**
     * Check if slot is occupied
     * @param slot Slot number
     * @return true if occupied
     */
    bool is_occupied(int slot) const;

    /**
     * Get sprite dimensions
     * @param slot Slot number
     * @param width Output width
     * @param height Output height
     * @return true if slot is valid
     */
    bool get_sprite_size(int slot, int& width, int& height) const;

    /**
     * Get number of occupied slots
     * @return Number of sprites loaded
     */
    int get_sprite_count() const;

    /**
     * Clear all sprites
     */
    void clear_all();

    /**
     * Process pending sprite load requests (called from main thread)
     * @return Number of sprites processed
     */
    int process_load_queue();

private:
    bool initialized_;
    SpriteSlot slots_[BANK_SIZE];
    mutable std::mutex bank_mutex_;
    int sprite_count_;
    
    // Loading queue for main thread processing
    std::queue<SpriteLoadRequest> load_queue_;
    std::mutex queue_mutex_;

    /**
     * Validate slot number
     * @param slot Slot number to check
     * @return true if valid
     */
    bool is_valid_slot(int slot) const;

    /**
     * Load PNG file to OpenGL texture using Cairo
     * @param filename PNG file path
     * @param texture_id Output texture ID
     * @param width Output image width
     * @param height Output image height
     * @return true on success
     */
    bool load_png_texture(const std::string& filename, GLuint& texture_id, 
                         int& width, int& height);

    /**
     * Create empty texture for missing sprites
     * @return OpenGL texture ID
     */
    GLuint create_empty_texture();

    /**
     * Actually load PNG file and create OpenGL texture (main thread only)
     * @param filename PNG file path
     * @param slot Sprite slot number
     * @return true on success
     */
    bool load_png_on_main_thread(const std::string& filename, int slot);

    /**
     * Create OpenGL textures for slots that need them (called from main thread)
     */
    void create_pending_textures();

public:
    /**
     * Save sprite texture as PNG file (for testing)
     * @param slot Sprite slot number
     * @param output_filename Output PNG file path
     * @return true on success
     */
    bool save_sprite_as_png(int slot, const std::string& output_filename);

    /**
     * Generate hash of sprite texture data (for verification)
     * @param slot Sprite slot number
     * @return Hash string or empty string if invalid
     */
    std::string hash_sprite_data(int slot);

    /**
     * Generate hash of PNG file contents (for comparison)
     * @param filename PNG file path
     * @return Hash string or empty string if file cannot be read
     */
    static std::string hash_png_file(const std::string& filename);
};

} // namespace AbstractRuntime

#endif // SPRITE_BANK_H