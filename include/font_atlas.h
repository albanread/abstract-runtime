#ifndef FONT_ATLAS_H
#define FONT_ATLAS_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

// Forward declarations for FreeType
using FT_Library = struct FT_LibraryRec_*;
using FT_Face = struct FT_FaceRec_*;

// Forward declarations for OpenGL
using GLuint = unsigned int;

namespace AbstractRuntime {

/**
 * Character metrics for rendered glyphs
 */
struct CharacterMetrics {
    int width = 0;           // Character width in pixels
    int height = 0;          // Character height in pixels  
    int bearing_x = 0;       // Left bearing (offset from cursor)
    int bearing_y = 0;       // Top bearing (offset from baseline)
    int advance = 0;         // Horizontal advance to next character
    
    // Atlas texture coordinates (normalized 0.0-1.0)
    float tex_x = 0.0f;      // Left edge in texture
    float tex_y = 0.0f;      // Top edge in texture
    float tex_w = 0.0f;      // Width in texture coordinates
    float tex_h = 0.0f;      // Height in texture coordinates
};

/**
 * Font loading configuration
 */
struct FontConfig {
    std::string font_path;
    int pixel_size = 16;             // Font size in pixels
    int atlas_width = 1024;          // Texture atlas width
    int atlas_height = 1024;         // Texture atlas height
    bool include_extended_ascii = true;  // Include chars 128-255
    bool include_petscii = true;     // Include PETSCII symbols
    bool include_unicode_blocks = true;  // Include box drawing, arrows, etc.
};

/**
 * FontAtlas manages pre-rendered font glyphs in a GPU texture.
 * For instant text rendering performance, all characters are rendered
 * to a texture atlas at initialization time.
 */
class FontAtlas {
public:
    FontAtlas();
    ~FontAtlas();

    /**
     * Initialize font atlas with TTF font
     * @param config Font configuration
     * @return true on success, false on failure
     */
    bool initialize(const FontConfig& config);

    /**
     * Shutdown and cleanup resources
     */
    void shutdown();

    /**
     * Get character metrics for text rendering
     * @param codepoint Unicode codepoint
     * @return Character metrics or nullptr if not available
     */
    const CharacterMetrics* get_character(uint32_t codepoint) const;

    /**
     * Get OpenGL texture ID for the atlas
     * @return OpenGL texture ID
     */
    GLuint get_texture_id() const { return atlas_texture_; }

    /**
     * Get font metrics
     */
    int get_line_height() const { return line_height_; }
    int get_baseline() const { return baseline_; }
    int get_char_width() const { return char_width_; }   // For monospace calculations
    int get_char_height() const { return char_height_; } // For grid calculations

    /**
     * Check if font is monospace (fixed-width)
     * @return true if monospace font
     */
    bool is_monospace() const { return is_monospace_; }

    /**
     * Calculate text dimensions
     * @param text Text string
     * @param width Output width in pixels
     * @param height Output height in pixels
     */
    void measure_text(const char* text, int& width, int& height) const;

    /**
     * Get atlas texture dimensions
     */
    int get_atlas_width() const { return atlas_width_; }
    int get_atlas_height() const { return atlas_height_; }

    /**
     * Debug: Save atlas texture to PNG file
     * @param filename Output file path
     * @return true on success
     */
    bool save_atlas_debug(const char* filename) const;

private:
    // FreeType resources
    FT_Library ft_library_;
    FT_Face ft_face_;
    
    // OpenGL resources
    GLuint atlas_texture_;
    int atlas_width_;
    int atlas_height_;
    
    // Font metrics
    int line_height_;
    int baseline_;
    int char_width_;     // Average/max character width
    int char_height_;    // Character cell height
    bool is_monospace_;
    
    // Character data
    std::unordered_map<uint32_t, CharacterMetrics> characters_;
    
    // Atlas packing state
    int pack_x_;
    int pack_y_;
    int pack_row_height_;
    
    // Atlas bitmap buffer (for building texture)
    std::vector<uint8_t> atlas_buffer_;

    /**
     * Load and render a single character to the atlas
     * @param codepoint Unicode codepoint to render
     * @return true on success
     */
    bool render_character(uint32_t codepoint);

    /**
     * Find space in atlas for character of given dimensions
     * @param width Character width
     * @param height Character height
     * @param out_x Output X position in atlas
     * @param out_y Output Y position in atlas
     * @return true if space found
     */
    bool pack_character(int width, int height, int& out_x, int& out_y);

    /**
     * Copy rendered glyph bitmap to atlas buffer
     * @param glyph_buffer Source bitmap data
     * @param glyph_width Glyph width
     * @param glyph_height Glyph height
     * @param atlas_x Destination X in atlas
     * @param atlas_y Destination Y in atlas
     */
    void copy_glyph_to_atlas(const uint8_t* glyph_buffer, 
                            int glyph_width, int glyph_height,
                            int atlas_x, int atlas_y);

    /**
     * Upload atlas buffer to GPU texture
     * @return true on success
     */
    bool upload_atlas_texture();

    /**
     * Load standard character ranges
     */
    bool load_ascii_characters();          // 32-126
    bool load_extended_ascii();            // 128-255
    bool load_petscii_characters();        // PETSCII symbols
    bool load_unicode_blocks();            // Box drawing, arrows, etc.

    /**
     * Calculate font metrics from loaded characters
     */
    void calculate_font_metrics();

    /**
     * Initialize FreeType library
     * @return true on success
     */
    bool initialize_freetype();

    /**
     * Load TTF font file
     * @param font_path Path to TTF file
     * @param pixel_size Font size in pixels
     * @return true on success
     */
    bool load_font_face(const std::string& font_path, int pixel_size);

    /**
     * Cleanup FreeType resources
     */
    void cleanup_freetype();

    /**
     * Unicode ranges for special character sets
     */
    static constexpr uint32_t PETSCII_START = 0xE000;    // Private use area
    static constexpr uint32_t PETSCII_COUNT = 256;
    
    static constexpr uint32_t BOX_DRAWING_START = 0x2500;
    static constexpr uint32_t BOX_DRAWING_COUNT = 128;
    
    static constexpr uint32_t ARROWS_START = 0x2190;
    static constexpr uint32_t ARROWS_COUNT = 112;
    
    static constexpr uint32_t MATH_START = 0x2200;
    static constexpr uint32_t MATH_COUNT = 256;

    // Character sets to load
    struct CharacterRange {
        uint32_t start;
        uint32_t count;
        const char* name;
    };
    
    static const CharacterRange standard_ranges_[];
    static const CharacterRange extended_ranges_[];
};

/**
 * Global font atlas instance for runtime use
 */
extern std::unique_ptr<FontAtlas> g_font_atlas;

/**
 * Initialize global font atlas with PetMe font
 * @param pixel_size Font size (8, 16, or 32 recommended)
 * @return true on success
 */
bool initialize_default_font_atlas(int pixel_size = 16);

/**
 * Shutdown global font atlas
 */
void shutdown_font_atlas();

/**
 * Quick access to global font atlas
 * @return Global font atlas instance or nullptr
 */
FontAtlas* get_font_atlas();

} // namespace AbstractRuntime

#endif // FONT_ATLAS_H