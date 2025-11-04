#include "font_atlas.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <OpenGL/gl3.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace AbstractRuntime {

// Global font atlas instance
std::unique_ptr<FontAtlas> g_font_atlas;

// Character ranges to load
const FontAtlas::CharacterRange FontAtlas::standard_ranges_[] = {
    {32, 95, "ASCII printable"},      // Space through ~
    {0, 0, nullptr}                   // Sentinel
};

const FontAtlas::CharacterRange FontAtlas::extended_ranges_[] = {
    {128, 128, "Extended ASCII"},     // Extended ASCII
    {0x2500, 128, "Box Drawing"},     // Box drawing characters
    {0x2190, 32, "Arrows"},           // Arrow symbols
    {0x2200, 64, "Math Symbols"},     // Mathematical symbols
    {0x25A0, 32, "Geometric"},        // Geometric shapes
    {0, 0, nullptr}                   // Sentinel
};

FontAtlas::FontAtlas()
    : ft_library_(nullptr)
    , ft_face_(nullptr)
    , atlas_texture_(0)
    , atlas_width_(1024)
    , atlas_height_(1024)
    , line_height_(16)
    , baseline_(12)
    , char_width_(8)
    , char_height_(16)
    , is_monospace_(true)
    , pack_x_(0)
    , pack_y_(0)
    , pack_row_height_(0) {
}

FontAtlas::~FontAtlas() {
    shutdown();
}

bool FontAtlas::initialize(const FontConfig& config) {
    atlas_width_ = config.atlas_width;
    atlas_height_ = config.atlas_height;
    
    // Initialize atlas buffer (single channel for alpha)
    atlas_buffer_.resize(atlas_width_ * atlas_height_, 0);
    
    // Initialize FreeType
    if (!initialize_freetype()) {
        std::cerr << "Failed to initialize FreeType" << std::endl;
        return false;
    }
    
    // Load font face
    if (!load_font_face(config.font_path, config.pixel_size)) {
        std::cerr << "Failed to load font: " << config.font_path << std::endl;
        return false;
    }
    
    // Reset packing state
    pack_x_ = 1;  // Leave 1 pixel border
    pack_y_ = 1;
    pack_row_height_ = 0;
    
    // Load standard ASCII characters
    if (!load_ascii_characters()) {
        std::cerr << "Failed to load ASCII characters" << std::endl;
        return false;
    }
    
    // Load extended character sets if requested
    if (config.include_extended_ascii) {
        load_extended_ascii();
    }
    
    if (config.include_unicode_blocks) {
        load_unicode_blocks();
    }
    
    // Calculate final font metrics
    calculate_font_metrics();
    
    // Upload texture to GPU
    if (!upload_atlas_texture()) {
        std::cerr << "Failed to upload font atlas texture" << std::endl;
        return false;
    }
    
    std::cout << "Font atlas initialized: " << characters_.size() 
              << " characters, " << char_width_ << "x" << char_height_ 
              << " cell size" << std::endl;
    
    return true;
}

void FontAtlas::shutdown() {
    if (atlas_texture_) {
        glDeleteTextures(1, &atlas_texture_);
        atlas_texture_ = 0;
    }
    
    cleanup_freetype();
    characters_.clear();
    atlas_buffer_.clear();
}

const CharacterMetrics* FontAtlas::get_character(uint32_t codepoint) const {
    auto it = characters_.find(codepoint);
    if (it != characters_.end()) {
        return &it->second;
    }
    
    // Return space character as fallback
    auto fallback = characters_.find(32);
    return (fallback != characters_.end()) ? &fallback->second : nullptr;
}

void FontAtlas::measure_text(const char* text, int& width, int& height) const {
    width = 0;
    height = line_height_;
    
    if (!text) return;
    
    int current_width = 0;
    int max_width = 0;
    int lines = 1;
    
    while (*text) {
        if (*text == '\n') {
            max_width = std::max(max_width, current_width);
            current_width = 0;
            lines++;
        } else {
            const CharacterMetrics* metrics = get_character(static_cast<uint8_t>(*text));
            if (metrics) {
                current_width += metrics->advance;
            }
        }
        text++;
    }
    
    width = std::max(max_width, current_width);
    height = lines * line_height_;
}

bool FontAtlas::save_atlas_debug(const char* filename) const {
    // Simple PPM format for debugging
    std::string ppm_file = std::string(filename) + ".ppm";
    FILE* file = fopen(ppm_file.c_str(), "wb");
    if (!file) return false;
    
    fprintf(file, "P5\n%d %d\n255\n", atlas_width_, atlas_height_);
    fwrite(atlas_buffer_.data(), 1, atlas_buffer_.size(), file);
    fclose(file);
    
    std::cout << "Font atlas debug saved to: " << ppm_file << std::endl;
    return true;
}

bool FontAtlas::render_character(uint32_t codepoint) {
    // Load character glyph
    FT_UInt glyph_index = FT_Get_Char_Index(ft_face_, codepoint);
    if (glyph_index == 0) {
        // Character not found in font
        return false;
    }
    
    // Render glyph
    if (FT_Load_Glyph(ft_face_, glyph_index, FT_LOAD_RENDER)) {
        return false;
    }
    
    FT_GlyphSlot glyph = ft_face_->glyph;
    FT_Bitmap& bitmap = glyph->bitmap;
    
    // Find space in atlas
    int atlas_x, atlas_y;
    if (!pack_character(bitmap.width + 2, bitmap.rows + 2, atlas_x, atlas_y)) {
        std::cerr << "No space in font atlas for character " << codepoint << std::endl;
        return false;
    }
    
    // Copy glyph to atlas (with 1 pixel border)
    copy_glyph_to_atlas(bitmap.buffer, bitmap.width, bitmap.rows, 
                       atlas_x + 1, atlas_y + 1);
    
    // Calculate texture coordinates
    CharacterMetrics metrics;
    metrics.width = bitmap.width;
    metrics.height = bitmap.rows;
    metrics.bearing_x = glyph->bitmap_left;
    metrics.bearing_y = glyph->bitmap_top;
    metrics.advance = glyph->advance.x >> 6;  // Convert from 26.6 fixed point
    
    metrics.tex_x = static_cast<float>(atlas_x) / atlas_width_;
    metrics.tex_y = static_cast<float>(atlas_y) / atlas_height_;
    metrics.tex_w = static_cast<float>(bitmap.width + 2) / atlas_width_;
    metrics.tex_h = static_cast<float>(bitmap.rows + 2) / atlas_height_;
    
    // Store character metrics
    characters_[codepoint] = metrics;
    
    return true;
}

bool FontAtlas::pack_character(int width, int height, int& out_x, int& out_y) {
    // Simple left-to-right, top-to-bottom packing
    if (pack_x_ + width > atlas_width_ - 1) {
        // Move to next row
        pack_x_ = 1;
        pack_y_ += pack_row_height_;
        pack_row_height_ = 0;
    }
    
    if (pack_y_ + height > atlas_height_ - 1) {
        // Atlas is full
        return false;
    }
    
    out_x = pack_x_;
    out_y = pack_y_;
    
    pack_x_ += width;
    pack_row_height_ = std::max(pack_row_height_, height);
    
    return true;
}

void FontAtlas::copy_glyph_to_atlas(const uint8_t* glyph_buffer,
                                   int glyph_width, int glyph_height,
                                   int atlas_x, int atlas_y) {
    for (int y = 0; y < glyph_height; ++y) {
        for (int x = 0; x < glyph_width; ++x) {
            int atlas_idx = (atlas_y + y) * atlas_width_ + (atlas_x + x);
            int glyph_idx = y * glyph_width + x;
            
            if (atlas_idx < atlas_buffer_.size() && glyph_idx < glyph_width * glyph_height) {
                atlas_buffer_[atlas_idx] = glyph_buffer[glyph_idx];
            }
        }
    }
}

bool FontAtlas::upload_atlas_texture() {
    // Generate OpenGL texture
    glGenTextures(1, &atlas_texture_);
    glBindTexture(GL_TEXTURE_2D, atlas_texture_);
    
    // Upload texture data (single channel alpha)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, atlas_width_, atlas_height_, 
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas_buffer_.data());
    
    // Set texture parameters for crisp pixel art
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error uploading font texture: " << error << std::endl;
        return false;
    }
    
    return true;
}

bool FontAtlas::load_ascii_characters() {
    // Load standard ASCII printable characters (32-126)
    for (uint32_t c = 32; c <= 126; ++c) {
        if (!render_character(c)) {
            std::cerr << "Failed to render ASCII character " << c << std::endl;
        }
    }
    
    // Ensure we have a space character for fallback
    if (characters_.find(32) == characters_.end()) {
        std::cerr << "Warning: No space character in font" << std::endl;
    }
    
    return true;
}

bool FontAtlas::load_extended_ascii() {
    // Load extended ASCII (128-255)
    for (uint32_t c = 128; c <= 255; ++c) {
        render_character(c);  // Don't fail if some characters missing
    }
    
    return true;
}

bool FontAtlas::load_petscii_characters() {
    // Load PETSCII characters if available in font
    for (uint32_t c = 0; c < 256; ++c) {
        render_character(PETSCII_START + c);
    }
    
    return true;
}

bool FontAtlas::load_unicode_blocks() {
    // Load extended Unicode ranges
    for (const CharacterRange* range = extended_ranges_; range->name; ++range) {
        for (uint32_t c = 0; c < range->count; ++c) {
            render_character(range->start + c);
        }
    }
    
    return true;
}

void FontAtlas::calculate_font_metrics() {
    if (characters_.empty()) return;
    
    // Calculate metrics from rendered characters
    int max_width = 0;
    int max_height = 0;
    int total_advance = 0;
    int char_count = 0;
    
    // Check if font is truly monospace
    int first_advance = -1;
    is_monospace_ = true;
    
    for (const auto& pair : characters_) {
        const CharacterMetrics& metrics = pair.second;
        
        max_width = std::max(max_width, metrics.width);
        max_height = std::max(max_height, metrics.height);
        total_advance += metrics.advance;
        char_count++;
        
        if (first_advance == -1) {
            first_advance = metrics.advance;
        } else if (metrics.advance != first_advance && pair.first != 32) {
            // Allow space to have different width
            is_monospace_ = false;
        }
    }
    
    // Set font metrics
    char_width_ = is_monospace_ ? first_advance : (total_advance / char_count);
    char_height_ = max_height;
    line_height_ = char_height_ + 2;  // Add some line spacing
    baseline_ = char_height_ * 3 / 4;  // Estimate baseline
    
    // For PetMe font, we know it's designed to be monospace
    if (is_monospace_ && char_width_ > 0) {
        std::cout << "Monospace font detected: " << char_width_ << "x" << char_height_ << std::endl;
    }
}

bool FontAtlas::initialize_freetype() {
    if (FT_Init_FreeType(&ft_library_)) {
        return false;
    }
    return true;
}

bool FontAtlas::load_font_face(const std::string& font_path, int pixel_size) {
    // Load font file
    if (FT_New_Face(ft_library_, font_path.c_str(), 0, &ft_face_)) {
        std::cerr << "Failed to load font file: " << font_path << std::endl;
        return false;
    }
    
    // Set font size
    if (FT_Set_Pixel_Sizes(ft_face_, 0, pixel_size)) {
        std::cerr << "Failed to set font size: " << pixel_size << std::endl;
        return false;
    }
    
    std::cout << "Loaded font: " << ft_face_->family_name 
              << " " << ft_face_->style_name 
              << " at " << pixel_size << "px" << std::endl;
    
    return true;
}

void FontAtlas::cleanup_freetype() {
    if (ft_face_) {
        FT_Done_Face(ft_face_);
        ft_face_ = nullptr;
    }
    
    if (ft_library_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
    }
}

// Global functions
bool initialize_default_font_atlas(int pixel_size) {
    if (g_font_atlas) {
        g_font_atlas->shutdown();
    }
    
    g_font_atlas = std::make_unique<FontAtlas>();
    
    FontConfig config;
    config.font_path = "assets/fonts/petme/PetMe.ttf";
    config.pixel_size = pixel_size;
    config.atlas_width = 1024;
    config.atlas_height = 1024;
    config.include_extended_ascii = true;
    config.include_petscii = false;  // PetMe may not have PETSCII in private use area
    config.include_unicode_blocks = true;
    
    if (!g_font_atlas->initialize(config)) {
        // Fallback to different PetMe variant
        config.font_path = "assets/fonts/petme/PetMe64.ttf";
        if (!g_font_atlas->initialize(config)) {
            std::cerr << "Failed to initialize default font atlas" << std::endl;
            g_font_atlas.reset();
            return false;
        }
    }
    
    return true;
}

void shutdown_font_atlas() {
    g_font_atlas.reset();
}

FontAtlas* get_font_atlas() {
    return g_font_atlas.get();
}

} // namespace AbstractRuntime