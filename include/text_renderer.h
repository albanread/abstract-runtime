#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "font_atlas.h"
#include <cstdint>
#include <vector>

// Forward declarations for OpenGL
typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace AbstractRuntime {

// Forward declarations
struct Layer;
struct TextData;
struct ViewportOffset;

/**
 * Text vertex for GPU rendering
 */
struct TextVertex {
    float x, y;           // Screen position
    float tex_x, tex_y;   // Texture coordinates
    uint32_t color;       // RGBA color
};

/**
 * Text render batch for efficient GPU submission
 */
struct TextBatch {
    std::vector<TextVertex> vertices;
    std::vector<uint16_t> indices;
    GLuint vertex_buffer;
    GLuint index_buffer;
    int character_count;
    
    TextBatch();
    ~TextBatch();
    
    void clear();
    void upload_to_gpu();
};

/**
 * TextRenderer handles immediate-mode text rendering using pre-rendered font atlas.
 * Provides instant text display with no rasterization delay.
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    /**
     * Initialize text renderer with font atlas
     * @param font_atlas Font atlas to use for rendering
     * @return true on success
     */
    bool initialize(FontAtlas* font_atlas);

    /**
     * Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * Render text layer to screen
     * @param text_data Text grid data
     * @param viewport_offset Layer viewport offset
     * @param screen_width Screen width in pixels
     * @param screen_height Screen height in pixels
     */
    void render_text_layer(const TextData& text_data, 
                          const ViewportOffset& viewport_offset,
                          int screen_width, int screen_height);

    /**
     * Render single text string at position
     * @param text Text string to render
     * @param x Screen X position
     * @param y Screen Y position  
     * @param color Text color (RGBA)
     * @param scale Text scale factor
     */
    void render_text_string(const char* text, int x, int y, 
                           uint32_t color, float scale = 1.0f);

    /**
     * Render text with background
     * @param text Text string
     * @param x Screen X position
     * @param y Screen Y position
     * @param text_color Text color
     * @param bg_color Background color
     * @param scale Text scale factor
     */
    void render_text_with_background(const char* text, int x, int y,
                                   uint32_t text_color, uint32_t bg_color,
                                   float scale = 1.0f);

    /**
     * Calculate text dimensions in pixels
     * @param text Text string
     * @param width Output width
     * @param height Output height
     * @param scale Text scale factor
     */
    void measure_text(const char* text, int& width, int& height, 
                     float scale = 1.0f);

    /**
     * Set text rendering properties
     */
    void set_blend_mode(bool enable_alpha_blend);
    void set_cursor_visible(bool visible);
    void set_cursor_position(int x, int y);
    void set_cursor_color(uint32_t color);
    void set_cursor_blink_rate(int milliseconds);

    /**
     * Grid-based text operations (for BCPL compatibility)
     */
    void render_text_at_grid(const char* text, int grid_x, int grid_y, 
                           uint32_t color);
    void clear_text_grid(int grid_x1, int grid_y1, int grid_x2, int grid_y2);
    void scroll_text_grid(int delta_x, int delta_y);

    /**
     * Font information
     */
    int get_char_width() const;
    int get_char_height() const;
    int get_line_height() const;
    int get_grid_cols(int screen_width) const;
    int get_grid_rows(int screen_height) const;

    /**
     * Debug and performance
     */
    void set_debug_mode(bool enable);
    int get_characters_rendered() const { return characters_rendered_; }
    float get_render_time_ms() const { return last_render_time_ms_; }

private:
    FontAtlas* font_atlas_;
    
    // OpenGL resources
    GLuint shader_program_;
    GLuint vertex_array_;
    TextBatch text_batch_;
    
    // Shader uniforms
    GLuint uniform_projection_;
    GLuint uniform_texture_;
    GLuint uniform_color_;
    
    // Rendering state
    bool initialized_;
    bool alpha_blend_enabled_;
    float projection_matrix_[16];
    
    // Cursor state
    bool cursor_visible_;
    int cursor_x_, cursor_y_;
    uint32_t cursor_color_;
    int cursor_blink_rate_ms_;
    uint64_t cursor_last_blink_time_;
    
    // Performance tracking
    int characters_rendered_;
    float last_render_time_ms_;
    bool debug_mode_;

    /**
     * Shader management
     */
    bool create_shader_program();
    bool compile_shader(GLenum type, const char* source, GLuint& shader);
    bool link_program(GLuint vertex_shader, GLuint fragment_shader);
    void destroy_shader_program();

    /**
     * Text batching
     */
    void begin_text_batch();
    void add_character_to_batch(uint32_t codepoint, int x, int y, 
                               uint32_t color, float scale);
    void add_quad_to_batch(float x, float y, float w, float h,
                          float tex_x, float tex_y, float tex_w, float tex_h,
                          uint32_t color);
    void flush_text_batch();

    /**
     * Coordinate conversion
     */
    void screen_to_ndc(int screen_x, int screen_y, float& ndc_x, float& ndc_y,
                      int screen_width, int screen_height);
    void grid_to_screen(int grid_x, int grid_y, int& screen_x, int& screen_y);

    /**
     * Cursor rendering
     */
    void render_cursor(int screen_width, int screen_height);
    bool should_cursor_blink();

    /**
     * Utility functions
     */
    uint64_t get_time_ms();
    void setup_projection_matrix(int screen_width, int screen_height);

    // Shader source code
    static const char* vertex_shader_source_;
    static const char* fragment_shader_source_;

    // Constants
    static constexpr int MAX_CHARACTERS_PER_BATCH = 1024;
    static constexpr float DEFAULT_CURSOR_BLINK_RATE = 500.0f; // ms
};

/**
 * Global text renderer instance
 */
extern std::unique_ptr<TextRenderer> g_text_renderer;

/**
 * Initialize global text renderer
 * @param font_atlas Font atlas to use
 * @return true on success
 */
bool initialize_text_renderer(FontAtlas* font_atlas);

/**
 * Shutdown global text renderer
 */
void shutdown_text_renderer();

/**
 * Get global text renderer instance
 * @return Text renderer or nullptr
 */
TextRenderer* get_text_renderer();

} // namespace AbstractRuntime

#endif // TEXT_RENDERER_H