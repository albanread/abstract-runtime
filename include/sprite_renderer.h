#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <cstdint>
#include <vector>
#include <mutex>

// Forward declarations for OpenGL
typedef unsigned int GLuint;

namespace AbstractRuntime {

// Forward declaration
class SpriteBank;

/**
 * Sprite instance information
 */
struct SpriteInstance {
    bool active = false;
    int sprite_slot = -1;  // Which sprite from the bank to use
    float x = 0.0f;        // X position in pixels
    float y = 0.0f;        // Y position in pixels
    float scale_x = 1.0f;  // Horizontal scale
    float scale_y = 1.0f;  // Vertical scale
    float rotation = 0.0f; // Rotation in degrees
    float alpha = 1.0f;    // Alpha transparency (0.0 - 1.0)
    int z_order = 0;       // Z-order for sorting (lower = behind)
    
    SpriteInstance() = default;
};

/**
 * SpriteRenderer manages up to 128 active sprite instances.
 * Handles positioning, visibility, and rendering of sprites from the sprite bank.
 */
class SpriteRenderer {
public:
    static constexpr int MAX_INSTANCES = 128;

    SpriteRenderer();
    ~SpriteRenderer();

    /**
     * Initialize sprite renderer
     * @param sprite_bank Pointer to sprite bank
     * @param screen_width Screen width in pixels
     * @param screen_height Screen height in pixels
     * @return true on success
     */
    bool initialize(SpriteBank* sprite_bank, int screen_width, int screen_height);

    /**
     * Shutdown and cleanup
     */
    void shutdown();

    /**
     * Create or update a sprite instance
     * @param instance_id Instance ID (0-127)
     * @param sprite_slot Sprite slot from bank (0-511)
     * @param x X position in pixels
     * @param y Y position in pixels
     * @return true on success
     */
    bool sprite(int instance_id, int sprite_slot, float x, float y);

    /**
     * Move existing sprite instance
     * @param instance_id Instance ID
     * @param x New X position
     * @param y New Y position
     * @return true on success
     */
    bool sprite_move(int instance_id, float x, float y);

    /**
     * Scale sprite instance
     * @param instance_id Instance ID
     * @param scale_x Horizontal scale factor
     * @param scale_y Vertical scale factor
     * @return true on success
     */
    bool sprite_scale(int instance_id, float scale_x, float scale_y);

    /**
     * Rotate sprite instance
     * @param instance_id Instance ID
     * @param degrees Rotation in degrees
     * @return true on success
     */
    bool sprite_rotate(int instance_id, float degrees);

    /**
     * Set sprite transparency
     * @param instance_id Instance ID
     * @param alpha Alpha value (0.0 = transparent, 1.0 = opaque)
     * @return true on success
     */
    bool sprite_alpha(int instance_id, float alpha);

    /**
     * Set sprite Z-order for layering
     * @param instance_id Instance ID
     * @param z_order Z order (lower = behind, higher = in front)
     * @return true on success
     */
    bool sprite_z_order(int instance_id, int z_order);

    /**
     * Hide sprite instance
     * @param instance_id Instance ID
     * @return true on success
     */
    bool sprite_hide(int instance_id);

    /**
     * Show sprite instance
     * @param instance_id Instance ID
     * @return true on success
     */
    bool sprite_show(int instance_id);

    /**
     * Check if sprite instance is visible
     * @param instance_id Instance ID
     * @return true if visible
     */
    bool sprite_is_visible(int instance_id) const;

    /**
     * Hide all sprites
     */
    void hide_all_sprites();

    /**
     * Get number of active sprite instances
     * @return Number of active sprites
     */
    int get_active_count() const;

    /**
     * Render all visible sprites to screen
     * This should be called during the render frame cycle
     */
    void render_sprites();

    /**
     * Update screen dimensions (for window resize)
     * @param width New screen width
     * @param height New screen height
     */
    void update_screen_size(int width, int height);

private:
    bool initialized_;
    SpriteBank* sprite_bank_;
    SpriteInstance instances_[MAX_INSTANCES];
    mutable std::mutex renderer_mutex_;
    int screen_width_;
    int screen_height_;
    int active_count_;

    /**
     * Validate instance ID
     * @param instance_id Instance ID to check
     * @return true if valid
     */
    bool is_valid_instance_id(int instance_id) const;

    /**
     * Check if sprite instance is on screen or partially visible
     * @param instance Instance to check
     * @param sprite_width Sprite width in pixels
     * @param sprite_height Sprite height in pixels
     * @return true if visible on screen
     */
    bool is_sprite_on_screen(const SpriteInstance& instance, 
                           int sprite_width, int sprite_height) const;

    /**
     * Render a single sprite instance
     * @param instance Sprite instance to render
     */
    void render_sprite_instance(const SpriteInstance& instance);

    /**
     * Render a single sprite instance (optimized - assumes texture already bound)
     * @param instance Sprite instance to render
     */
    void render_sprite_instance_optimized(const SpriteInstance& instance);

    /**
     * Get sorted list of visible sprites by z-order
     * @return Vector of instance IDs sorted by z-order
     */
    std::vector<int> get_sorted_visible_sprites() const;
};

} // namespace AbstractRuntime

#endif // SPRITE_RENDERER_H