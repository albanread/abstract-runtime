#include "tile_layer.h"
#include "sprite_bank.h"
#include <OpenGL/gl.h>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace AbstractRuntime {

TileLayer::TileLayer()
    : initialized_(false)
    , screen_width_(800)
    , screen_height_(600)
    , grid_width_(0)
    , grid_height_(0)
    , scroll_x_(0.0f)
    , scroll_y_(0.0f)
    , visible_start_x_(0)
    , visible_start_y_(0)
    , visible_end_x_(0)
    , visible_end_y_(0) {
}

TileLayer::~TileLayer() {
    shutdown();
}

bool TileLayer::initialize(int screen_width, int screen_height) {
    if (initialized_) {
        return true;
    }
    
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    
    // Create tile bank for texture management
    tile_bank_ = std::make_unique<SpriteBank>();
    if (!tile_bank_->initialize()) {
        std::cerr << "TileLayer: Failed to initialize tile bank" << std::endl;
        return false;
    }
    
    // Calculate grid dimensions (includes border tiles for scrolling)
    calculate_grid_dimensions();
    
    // Initialize grid with empty tiles (0)
    grid_.resize(grid_height_);
    for (int y = 0; y < grid_height_; ++y) {
        grid_[y].resize(grid_width_, 0);
    }
    
    // Initialize visible region
    update_visible_region();
    
    initialized_ = true;
    
    std::cout << "TileLayer initialized:" << std::endl;
    std::cout << "  Screen: " << screen_width_ << "x" << screen_height_ << std::endl;
    std::cout << "  Grid: " << grid_width_ << "x" << grid_height_ << " tiles" << std::endl;
    std::cout << "  Tile size: " << TILE_SIZE << "x" << TILE_SIZE << " pixels" << std::endl;
    
    return true;
}

void TileLayer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (tile_bank_) {
        tile_bank_->shutdown();
        tile_bank_.reset();
    }
    
    grid_.clear();
    initialized_ = false;
    
    std::cout << "TileLayer shutdown complete" << std::endl;
}

void TileLayer::calculate_grid_dimensions() {
    // Calculate how many tiles we need to cover the screen
    int tiles_x = (screen_width_ + TILE_SIZE - 1) / TILE_SIZE;  // Ceiling division
    int tiles_y = (screen_height_ + TILE_SIZE - 1) / TILE_SIZE;
    
    // Add one extra tile around all edges for smooth scrolling
    grid_width_ = tiles_x + 2;
    grid_height_ = tiles_y + 2;
}

void TileLayer::update_visible_region() {
    // Calculate which tiles are currently visible based on scroll offset
    visible_start_x_ = std::max(0, (int)floor(scroll_x_ / TILE_SIZE));
    visible_start_y_ = std::max(0, (int)floor(scroll_y_ / TILE_SIZE));
    
    visible_end_x_ = std::min(grid_width_ - 1, 
                             visible_start_x_ + (screen_width_ + TILE_SIZE - 1) / TILE_SIZE + 1);
    visible_end_y_ = std::min(grid_height_ - 1, 
                             visible_start_y_ + (screen_height_ + TILE_SIZE - 1) / TILE_SIZE + 1);
}

bool TileLayer::load_tile(int tile_id, const std::string& filename) {
    if (!initialized_) {
        std::cerr << "TileLayer: Not initialized" << std::endl;
        return false;
    }
    
    if (tile_id <= 0 || tile_id >= MAX_TILES) {
        std::cerr << "TileLayer: Invalid tile ID " << tile_id << " (must be 1-" << (MAX_TILES-1) << ")" << std::endl;
        return false;
    }
    
    // Use sprite bank to load the tile texture
    if (!tile_bank_->load_sprite(tile_id, filename)) {
        std::cerr << "TileLayer: Failed to load tile " << tile_id << " from " << filename << std::endl;
        return false;
    }
    
    std::cout << "TileLayer: Loaded tile " << tile_id << " from " << filename << std::endl;
    return true;
}

void TileLayer::unload_tile(int tile_id) {
    if (!initialized_ || tile_id <= 0 || tile_id >= MAX_TILES) {
        return;
    }
    
    tile_bank_->release_sprite(tile_id);
}

void TileLayer::unload_all_tiles() {
    if (!initialized_) {
        return;
    }
    
    for (int i = 1; i < MAX_TILES; ++i) {
        tile_bank_->release_sprite(i);
    }
}

void TileLayer::set_grid_size(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) {
        return;
    }
    
    grid_width_ = width;
    grid_height_ = height;
    
    // Resize grid
    grid_.resize(grid_height_);
    for (int y = 0; y < grid_height_; ++y) {
        grid_[y].resize(grid_width_, 0);
    }
    
    update_visible_region();
}

void TileLayer::get_grid_size(int& width, int& height) const {
    width = grid_width_;
    height = grid_height_;
}

bool TileLayer::is_valid_grid_position(int grid_x, int grid_y) const {
    return grid_x >= 0 && grid_x < grid_width_ && 
           grid_y >= 0 && grid_y < grid_height_;
}

void TileLayer::set_tile(int grid_x, int grid_y, int tile_id) {
    if (!initialized_ || !is_valid_grid_position(grid_x, grid_y)) {
        return;
    }
    
    if (tile_id < 0 || tile_id >= MAX_TILES) {
        tile_id = 0;  // Clamp to empty tile
    }
    
    grid_[grid_y][grid_x] = tile_id;
}

int TileLayer::get_tile(int grid_x, int grid_y) const {
    if (!initialized_ || !is_valid_grid_position(grid_x, grid_y)) {
        return 0;
    }
    
    return grid_[grid_y][grid_x];
}

void TileLayer::clear_grid() {
    if (!initialized_) {
        return;
    }
    
    for (int y = 0; y < grid_height_; ++y) {
        for (int x = 0; x < grid_width_; ++x) {
            grid_[y][x] = 0;
        }
    }
}

void TileLayer::fill_grid(int tile_id) {
    if (!initialized_) {
        return;
    }
    
    if (tile_id < 0 || tile_id >= MAX_TILES) {
        tile_id = 0;
    }
    
    for (int y = 0; y < grid_height_; ++y) {
        for (int x = 0; x < grid_width_; ++x) {
            grid_[y][x] = tile_id;
        }
    }
}

void TileLayer::fill_rect(int start_x, int start_y, int width, int height, int tile_id) {
    if (!initialized_) {
        return;
    }
    
    if (tile_id < 0 || tile_id >= MAX_TILES) {
        tile_id = 0;
    }
    
    int end_x = std::min(grid_width_, start_x + width);
    int end_y = std::min(grid_height_, start_y + height);
    
    for (int y = std::max(0, start_y); y < end_y; ++y) {
        for (int x = std::max(0, start_x); x < end_x; ++x) {
            grid_[y][x] = tile_id;
        }
    }
}

void TileLayer::draw_border(int tile_id, int border_width) {
    if (!initialized_ || border_width <= 0) {
        return;
    }
    
    if (tile_id < 0 || tile_id >= MAX_TILES) {
        tile_id = 0;
    }
    
    // Top and bottom borders
    for (int y = 0; y < border_width && y < grid_height_; ++y) {
        for (int x = 0; x < grid_width_; ++x) {
            grid_[y][x] = tile_id;  // Top
            if (grid_height_ - 1 - y >= 0) {
                grid_[grid_height_ - 1 - y][x] = tile_id;  // Bottom
            }
        }
    }
    
    // Left and right borders
    for (int x = 0; x < border_width && x < grid_width_; ++x) {
        for (int y = 0; y < grid_height_; ++y) {
            grid_[y][x] = tile_id;  // Left
            if (grid_width_ - 1 - x >= 0) {
                grid_[y][grid_width_ - 1 - x] = tile_id;  // Right
            }
        }
    }
}

void TileLayer::set_scroll_offset(float x, float y) {
    scroll_x_ = x;
    scroll_y_ = y;
    update_visible_region();
}

void TileLayer::get_scroll_offset(float& x, float& y) const {
    x = scroll_x_;
    y = scroll_y_;
}

void TileLayer::scroll_by(float dx, float dy) {
    scroll_x_ += dx;
    scroll_y_ += dy;
    update_visible_region();
}

void TileLayer::world_to_grid(float world_x, float world_y, int& grid_x, int& grid_y) const {
    grid_x = (int)floor((world_x + scroll_x_) / TILE_SIZE);
    grid_y = (int)floor((world_y + scroll_y_) / TILE_SIZE);
}

void TileLayer::grid_to_world(int grid_x, int grid_y, float& world_x, float& world_y) const {
    world_x = grid_x * TILE_SIZE - scroll_x_;
    world_y = grid_y * TILE_SIZE - scroll_y_;
}

void TileLayer::render_tile(int grid_x, int grid_y, int tile_id) {
    if (tile_id <= 0) {
        return;  // Empty tile, nothing to draw
    }
    
    // Get tile texture info from sprite bank
    GLuint texture = tile_bank_->get_texture(tile_id);
    if (texture == 0) {
        return;  // Texture not loaded
    }
    
    // Calculate screen position
    float world_x, world_y;
    grid_to_world(grid_x, grid_y, world_x, world_y);
    
    // Convert to screen coordinates
    float screen_x = world_x;
    float screen_y = world_y;
    
    // Bind texture
    glBindTexture(GL_TEXTURE_2D, texture);
    
    // Draw tile as a textured quad
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(screen_x, screen_y);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(screen_x + TILE_SIZE, screen_y);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(screen_x + TILE_SIZE, screen_y + TILE_SIZE);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(screen_x, screen_y + TILE_SIZE);
    glEnd();
}

void TileLayer::render() {
    if (!initialized_) {
        return;
    }
    
    // Set up OpenGL state for tile rendering
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set up 2D projection matrix
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Render only visible tiles for performance
    update_visible_region();
    
    int tiles_rendered = 0;
    
    for (int grid_y = visible_start_y_; grid_y <= visible_end_y_; ++grid_y) {
        for (int grid_x = visible_start_x_; grid_x <= visible_end_x_; ++grid_x) {
            if (is_valid_grid_position(grid_x, grid_y)) {
                int tile_id = grid_[grid_y][grid_x];
                if (tile_id > 0) {
                    render_tile(grid_x, grid_y, tile_id);
                    tiles_rendered++;
                }
            }
        }
    }
    
    // Restore OpenGL state
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    
    // Debug output (can be removed for production)
    static int frame_count = 0;
    if (++frame_count % 60 == 0) {  // Print every second at 60fps
        std::cout << "TileLayer: Rendered " << tiles_rendered << " tiles" << std::endl;
    }
}

int TileLayer::get_tiles_loaded() const {
    if (!initialized_) {
        return 0;
    }
    
    int count = 0;
    for (int i = 1; i < MAX_TILES; ++i) {
        if (tile_bank_->get_texture(i) != 0) {
            count++;
        }
    }
    return count;
}

void TileLayer::print_grid_info() const {
    if (!initialized_) {
        std::cout << "TileLayer: Not initialized" << std::endl;
        return;
    }
    
    std::cout << "TileLayer Grid Info:" << std::endl;
    std::cout << "  Screen: " << screen_width_ << "x" << screen_height_ << std::endl;
    std::cout << "  Grid: " << grid_width_ << "x" << grid_height_ << " tiles" << std::endl;
    std::cout << "  Tile size: " << TILE_SIZE << "x" << TILE_SIZE << " pixels" << std::endl;
    std::cout << "  Scroll offset: (" << scroll_x_ << ", " << scroll_y_ << ")" << std::endl;
    std::cout << "  Visible region: (" << visible_start_x_ << "," << visible_start_y_ 
              << ") to (" << visible_end_x_ << "," << visible_end_y_ << ")" << std::endl;
    std::cout << "  Tiles loaded: " << get_tiles_loaded() << std::endl;
    
    // Count non-empty tiles in grid
    int non_empty_tiles = 0;
    for (int y = 0; y < grid_height_; ++y) {
        for (int x = 0; x < grid_width_; ++x) {
            if (grid_[y][x] > 0) {
                non_empty_tiles++;
            }
        }
    }
    std::cout << "  Non-empty grid positions: " << non_empty_tiles << std::endl;
}

} // namespace AbstractRuntime