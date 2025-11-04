#ifndef TILE_LAYER_H
#define TILE_LAYER_H

#include <vector>
#include <string>
#include <memory>

namespace AbstractRuntime {

// Forward declarations
class SpriteBank;

/**
 * TileLayer - Grid-based tile rendering system for backgrounds
 * 
 * Manages a 2D grid of tiles where each tile is 128x128 pixels.
 * Tiles are positioned on a regular grid that fills the screen with
 * one extra tile around all edges for smooth scrolling.
 * 
 * Tile ID 0 = empty (nothing drawn)
 * Other tile IDs reference PNG files loaded into the tile bank
 */
class TileLayer {
public:
    static const int TILE_SIZE = 128;           // Each tile is 128x128 pixels
    static const int MAX_TILES = 256;           // Maximum tile types supported
    
    TileLayer();
    ~TileLayer();
    
    // Initialization and shutdown
    bool initialize(int screen_width, int screen_height);
    void shutdown();
    bool is_initialized() const { return initialized_; }
    
    // Tile management
    bool load_tile(int tile_id, const std::string& filename);
    void unload_tile(int tile_id);
    void unload_all_tiles();
    
    // Grid management
    void set_grid_size(int width, int height);
    void get_grid_size(int& width, int& height) const;
    
    // Tile placement
    void set_tile(int grid_x, int grid_y, int tile_id);
    int get_tile(int grid_x, int grid_y) const;
    void clear_grid();
    void fill_grid(int tile_id);
    
    // Scrolling support
    void set_scroll_offset(float x, float y);
    void get_scroll_offset(float& x, float& y) const;
    void scroll_by(float dx, float dy);
    
    // Pattern filling
    void fill_rect(int start_x, int start_y, int width, int height, int tile_id);
    void draw_border(int tile_id, int border_width = 1);
    
    // Rendering
    void render();
    
    // Screen coordinate conversion
    void world_to_grid(float world_x, float world_y, int& grid_x, int& grid_y) const;
    void grid_to_world(int grid_x, int grid_y, float& world_x, float& world_y) const;
    
    // Utility
    int get_tiles_loaded() const;
    void print_grid_info() const;
    
private:
    bool initialized_;
    
    // Screen dimensions
    int screen_width_;
    int screen_height_;
    
    // Grid dimensions (includes extra border tiles for scrolling)
    int grid_width_;
    int grid_height_;
    
    // Tile storage
    std::vector<std::vector<int>> grid_;  // [y][x] tile IDs
    
    // Tile textures (using SpriteBank for texture management)
    std::unique_ptr<SpriteBank> tile_bank_;
    
    // Scrolling
    float scroll_x_;
    float scroll_y_;
    
    // Rendering optimization
    int visible_start_x_;
    int visible_start_y_;
    int visible_end_x_;
    int visible_end_y_;
    
    // Helper methods
    void calculate_grid_dimensions();
    void update_visible_region();
    bool is_valid_grid_position(int grid_x, int grid_y) const;
    void render_tile(int grid_x, int grid_y, int tile_id);
};

} // namespace AbstractRuntime

#endif // TILE_LAYER_H