#ifndef LAYER_RENDERER_H
#define LAYER_RENDERER_H

#include <cstdint>
#include <vector>

// Forward declarations for OpenGL
typedef unsigned int GLuint;

namespace AbstractRuntime {

// Forward declarations
struct Layer;
struct SpriteInstance;
class SpriteBank;
struct TextData;
struct TileData;
struct ViewportOffset;
class FontAtlas;

/**
 * LayerRenderer handles GPU rendering for all layer types.
 * Provides efficient OpenGL-based composition of the layer stack.
 */
class LayerRenderer {
public:
    LayerRenderer();
    ~LayerRenderer();

    /**
     * Initialize renderer with screen dimensions
     * @param screen_width Screen width in pixels
     * @param screen_height Screen height in pixels
     * @return true on success
     */
    bool initialize(int screen_width, int screen_height);

    /**
     * Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * Resize renderer for new screen dimensions
     * @param screen_width New screen width in pixels
     * @param screen_height New screen height in pixels
     */
    void resize(int screen_width, int screen_height);

    /**
     * Render background sprite
     * @param sprite_slot Sprite slot to use as background
     * @param screen_width Screen width
     * @param screen_height Screen height
     */
    void render_background_sprite(int sprite_slot, int screen_width, int screen_height);

    /**
     * Render tile layer
     * @param tile_data Tile map data
     * @param viewport_offset Layer viewport offset
     * @param screen_width Screen width
     * @param screen_height Screen height
     * @param sprite_bank Sprite bank for tile textures
     */
    void render_tiles(const TileData& tile_data,
                     const ViewportOffset& viewport_offset,
                     int screen_width, int screen_height,
                     const SpriteBank& sprite_bank);

    /**
     * Render graphics commands (vector graphics)
     * @param commands List of graphics commands
     * @param viewport_offset Layer viewport offset
     * @param screen_width Screen width
     * @param screen_height Screen height
     */
    void render_graphics(const std::vector<struct GraphicsCommand>& commands,
                        const ViewportOffset& viewport_offset,
                        int screen_width, int screen_height);

    /**
     * Render sprite instances
     * @param sprites Array of sprite instances
     * @param sprite_count Number of sprites
     * @param sprite_bank Sprite bank for textures
     */
    void render_sprites(const SpriteInstance* sprites, int sprite_count,
                       const SpriteBank& sprite_bank);

    /**
     * Render text layer
     * @param text_data Text grid data
     * @param viewport_offset Layer viewport offset
     * @param screen_width Screen width
     * @param screen_height Screen height
     * @param font_atlas Font atlas for character textures
     */
    void render_text(const TextData& text_data,
                    const ViewportOffset& viewport_offset,
                    int screen_width, int screen_height,
                    const FontAtlas& font_atlas);

private:
    bool initialized_;
    int screen_width_;
    int screen_height_;
    
    // OpenGL resources (minimal for now)
    GLuint dummy_shader_;
    GLuint dummy_vao_;
    GLuint dummy_vbo_;

    /**
     * Initialize OpenGL resources
     */
    bool init_gl_resources();

    /**
     * Cleanup OpenGL resources
     */
    void cleanup_gl_resources();

    /**
     * Render a placeholder character (colored rectangle)
     * @param x Screen X position
     * @param y Screen Y position
     * @param width Character width
     * @param height Character height
     * @param r Red component (0.0-1.0)
     * @param g Green component (0.0-1.0)
     * @param b Blue component (0.0-1.0)
     * @param a Alpha component (0.0-1.0)
     */
    void render_text_character_placeholder(int x, int y, int width, int height, float r, float g, float b, float a);
};

} // namespace AbstractRuntime

#endif // LAYER_RENDERER_H