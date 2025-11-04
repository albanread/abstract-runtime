#include "layer_renderer.h"
#include "runtime_state.h"
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <iostream>

namespace AbstractRuntime {

LayerRenderer::LayerRenderer()
    : initialized_(false)
    , screen_width_(0)
    , screen_height_(0)
    , dummy_shader_(0)
    , dummy_vao_(0)
    , dummy_vbo_(0) {
}

LayerRenderer::~LayerRenderer() {
    shutdown();
}

bool LayerRenderer::initialize(int screen_width, int screen_height) {
    if (initialized_) {
        return true;
    }
    
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    
    if (!init_gl_resources()) {
        std::cerr << "Failed to initialize OpenGL resources" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "LayerRenderer initialized: " << screen_width << "x" << screen_height << std::endl;
    return true;
}

void LayerRenderer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    cleanup_gl_resources();
    initialized_ = false;
}

void LayerRenderer::resize(int screen_width, int screen_height) {
    if (!initialized_) {
        return;
    }
    
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    
    // Update OpenGL viewport
    glViewport(0, 0, screen_width, screen_height);
    
    // TODO: Update any framebuffers or render targets that depend on screen size
    std::cout << "LayerRenderer resized to: " << screen_width << "x" << screen_height << std::endl;
}

void LayerRenderer::render_background_sprite(int sprite_slot, int screen_width, int screen_height) {
    // TODO: Implement background sprite rendering
    (void)sprite_slot;
    (void)screen_width;
    (void)screen_height;
}

void LayerRenderer::render_tiles(const TileData& tile_data,
                                const ViewportOffset& viewport_offset,
                                int screen_width, int screen_height,
                                const SpriteBank& sprite_bank) {
    // TODO: Implement tile rendering
    (void)tile_data;
    (void)viewport_offset;
    (void)screen_width;
    (void)screen_height;
    (void)sprite_bank;
}

void LayerRenderer::render_graphics(const std::vector<GraphicsCommand>& commands,
                                   const ViewportOffset& viewport_offset,
                                   int screen_width, int screen_height) {
    // TODO: Implement vector graphics rendering
    (void)commands;
    (void)viewport_offset;
    (void)screen_width;
    (void)screen_height;
}

void LayerRenderer::render_sprites(const SpriteInstance* sprites, int sprite_count,
                                  const SpriteBank& sprite_bank) {
    // TODO: Implement sprite rendering
    (void)sprites;
    (void)sprite_count;
    (void)sprite_bank;
}

void LayerRenderer::render_text(const TextData& text_data,
                               const ViewportOffset& viewport_offset,
                               int screen_width, int screen_height,
                               const FontAtlas& font_atlas) {
    if (!initialized_) {
        return;
    }
    
    // Character dimensions (8x16 pixels)
    const int char_width = 8;
    const int char_height = 16;
    
    // Simple text rendering - draw each character as a colored rectangle for now
    // TODO: Use actual font atlas textures when font system is fully implemented
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render each character in the text buffer
    for (int y = 0; y < text_data.rows; y++) {
        for (int x = 0; x < text_data.cols; x++) {
            int buffer_index = y * text_data.cols + x;
            
            if (buffer_index >= (int)text_data.characters.size()) break;
            
            char ch = text_data.characters[buffer_index];
            if (ch == ' ' || ch == '\0') continue;  // Skip spaces
            
            uint32_t color = text_data.colors[buffer_index];
            
            // Extract color components
            float r = ((color >> 24) & 0xFF) / 255.0f;
            float g = ((color >> 16) & 0xFF) / 255.0f;
            float b = ((color >> 8) & 0xFF) / 255.0f;
            float a = (color & 0xFF) / 255.0f;
            
            // Calculate screen position
            int screen_x = x * char_width - viewport_offset.x;
            int screen_y = y * char_height - viewport_offset.y;
            
            // Skip characters outside screen bounds
            if (screen_x + char_width < 0 || screen_x >= screen_width ||
                screen_y + char_height < 0 || screen_y >= screen_height) {
                continue;
            }
            
            // Render character as a simple colored rectangle (placeholder)
            // This gives immediate visual feedback that text positioning works
            render_text_character_placeholder(screen_x, screen_y, char_width, char_height, r, g, b, a);
        }
    }
}

void LayerRenderer::render_text_character_placeholder(int x, int y, int width, int height, float r, float g, float b, float a) {
    // Set up orthographic projection for screen coordinates
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Use immediate mode OpenGL with screen coordinates
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    
    // Draw filled rectangle using screen coordinates
    glBegin(GL_QUADS);
        glVertex2f(x, y);               // Top left
        glVertex2f(x + width, y);       // Top right  
        glVertex2f(x + width, y + height); // Bottom right
        glVertex2f(x, y + height);      // Bottom left
    glEnd();
}

bool LayerRenderer::init_gl_resources() {
    // OpenGL 2.1 compatible initialization - no VAOs needed
    // Just initialize basic state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error in LayerRenderer init: " << error << std::endl;
        return false;
    }
    
    std::cout << "LayerRenderer OpenGL 2.1 resources initialized successfully" << std::endl;
    return true;
}

void LayerRenderer::cleanup_gl_resources() {
    // No OpenGL 2.1 resources to clean up for now
    std::cout << "LayerRenderer: OpenGL 2.1 resources cleaned up" << std::endl;
}

} // namespace AbstractRuntime