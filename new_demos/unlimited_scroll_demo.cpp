#include "../include/abstract_runtime.h"
#include "../include/input_system.h"
#include <cmath>
#include <cstdio>
#include <chrono>
#include <thread>

using namespace AbstractRuntime;

/**
 * Modern Unlimited Scrolling Demo
 * 
 * Demonstrates unlimited tile-based scrolling with viewport shifting,
 * smooth sub-tile movement, and performance optimization.
 * 
 * Features:
 * - Large world map (60x60 tiles)
 * - Smooth scrolling beyond viewport boundaries
 * - Visual feedback for viewport shifting
 * - Performance monitoring
 * - Interactive controls
 */

class UnlimitedScrollDemo {
private:
    static constexpr int WORLD_WIDTH = 60;
    static constexpr int WORLD_HEIGHT = 60;
    static constexpr int TILE_SIZE = 128;
    static constexpr float SCROLL_SPEED = 2.0f;
    static constexpr float FAST_SCROLL_SPEED = 8.0f;
    
    int frame_count = 0;
    bool auto_scroll = true;
    bool show_debug = true;
    float manual_scroll_x = 0.0f;
    float manual_scroll_y = 0.0f;
    float world_center_x;
    float world_center_y;
    
public:
    UnlimitedScrollDemo() {
        world_center_x = (WORLD_WIDTH / 2.0f) * TILE_SIZE;
        world_center_y = (WORLD_HEIGHT / 2.0f) * TILE_SIZE;
    }
    
    void initialize() {
        console_section("Unlimited Scroll Demo Initialization");
        
        // Initialize input system explicitly
        console_info("Initializing input system...");
        if (!init_input_system()) {
            console_error("Failed to initialize input system!");
            exit_runtime();
            return;
        }
        console_info("Input system initialized successfully");
        
        // Set up world map size BEFORE initializing tiles
        console_printf("Setting up world map (%dx%d tiles)...", WORLD_WIDTH, WORLD_HEIGHT);
        if (!set_world_map_size(WORLD_WIDTH, WORLD_HEIGHT)) {
            console_error("Failed to set world map size!");
            exit_runtime();
            return;
        }
        console_info("World map configured successfully");
        
        // Now initialize tile system
        console_info("Initializing tile system...");
        if (!init_tiles()) {
            console_error("Failed to initialize tile system!");
            exit_runtime();
            return;
        }
        console_info("Tile system initialized successfully");
        
        // Load tile assets
        console_info("Loading tile assets...");
        if (!load_tiles()) {
            console_warning("Some tile assets failed to load (will show colored squares)");
        } else {
            console_info("All tile assets loaded successfully");
        }
        
        // Create world pattern
        console_info("Generating world pattern...");
        create_world_pattern();
        console_info("World pattern generated successfully");
        
        // Set initial position at world center
        manual_scroll_x = world_center_x;
        manual_scroll_y = world_center_y;
        set_tile_scroll(manual_scroll_x, manual_scroll_y);
        console_printf("Starting position: (%.1f, %.1f)", manual_scroll_x, manual_scroll_y);
        
        console_info("Demo initialization complete!");
        
        // Input system ready
        console_info("Input system ready - key() for control keys, is_key_pressed() for arrows");
        console_separator();
    }
    
private:
    bool load_tiles() {
        struct TileAsset {
            int id;
            const char* filename;
            const char* name;
        };
        
        TileAsset tiles[] = {
            {1, "assets/tiles/tile001_sky_light.png", "Sky"},
            {2, "assets/tiles/tile008_grass_green.png", "Grass"},
            {3, "assets/tiles/tile011_rock_grey.png", "Rock"},
            {4, "assets/tiles/tile005_water_deep.png", "Water"},
            {5, "assets/tiles/tile015_dirt_brown.png", "Dirt"},
            {6, "assets/tiles/tile020_sand_yellow.png", "Sand"}
        };
        
        bool all_loaded = true;
        for (const auto& tile : tiles) {
            if (load_tile(tile.id, tile.filename)) {
                console_printf("  Loaded %s tile (ID %d)", tile.name, tile.id);
            } else {
                console_printf("  Failed to load %s tile (%s)", tile.name, tile.filename);
                all_loaded = false;
            }
        }
        
        return all_loaded;
    }
    
    void create_world_pattern() {
        clear_tiles();
        
        console_printf("  Generating %dx%d tile pattern...", WORLD_WIDTH, WORLD_HEIGHT);
        int tiles_generated = 0;
        
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int x = 0; x < WORLD_WIDTH; x++) {
                int tile_id = calculate_tile_id(x, y);
                set_tile(x, y, tile_id);
                tiles_generated++;
            }
        }
        
        console_printf("  Generated %d tiles with pattern", tiles_generated);
    }
    
    int calculate_tile_id(int x, int y) {
        // Create distinct regions for easy visual identification
        
        // Grid lines every 10 tiles for reference
        if (x % 10 == 0 || y % 10 == 0) {
            return 3; // Rock grid lines
        }
        
        // Diagonal stripes for movement visibility  
        if ((x + y) % 8 == 0) {
            return 6; // Sand stripes
        }
        
        // Regional patterns based on position
        int region_x = x / 10;
        int region_y = y / 10;
        
        // Create a checkerboard of different tile types
        if ((region_x + region_y) % 2 == 0) {
            return 2; // Grass regions
        } else {
            return 4; // Water regions
        }
    }
    
public:
    void update() {
        handle_input();
        
        if (auto_scroll) {
            update_auto_scroll();
        }
        
        // Apply current scroll position
        set_tile_scroll(manual_scroll_x, manual_scroll_y);
        
        // Update display
        update_display();
        
        frame_count++;
    }
private:
    void handle_input() {
        // Handle control keys using key() function (works for ASCII keys)
        int current_key = key();
        static int last_key = -1;
        static bool key_handled = false;
        
        // Only process key if it's different from last frame (simulates key_just_pressed)
        if (current_key != last_key) {
            key_handled = false;
            
            if (current_key != -1 && !key_handled) {
                // Handle control keys
                if (current_key == 32) { // Space
                    auto_scroll = !auto_scroll;
                    console_printf("Auto-scroll %s", auto_scroll ? "ENABLED" : "DISABLED");
                    key_handled = true;
                }
                else if (current_key == 9) { // Tab
                    show_debug = !show_debug;
                    console_printf("Debug info %s", show_debug ? "SHOWN" : "HIDDEN");
                    key_handled = true;
                }
                else if (current_key == 27) { // Escape
                    console_info("Escape pressed - exiting demo");
                    exit_runtime();
                    return;
                }
            }
        }
        
        // Handle arrow key scrolling using is_key_pressed() (works for arrow keys)
        if (!auto_scroll) {
            float speed = SCROLL_SPEED;
            bool moved = false;
            
            if (is_key_pressed(INPUT_KEY_LEFT)) {
                manual_scroll_x -= speed;
                moved = true;
            }
            if (is_key_pressed(INPUT_KEY_RIGHT)) {
                manual_scroll_x += speed;
                moved = true;
            }
            if (is_key_pressed(INPUT_KEY_UP)) {
                manual_scroll_y -= speed;
                moved = true;
            }
            if (is_key_pressed(INPUT_KEY_DOWN)) {
                manual_scroll_y += speed;
                moved = true;
            }
            
            // Keep within world bounds
            if (moved) {
                float max_x = (WORLD_WIDTH - 1) * TILE_SIZE;
                float max_y = (WORLD_HEIGHT - 1) * TILE_SIZE;
                
                if (manual_scroll_x < 0) manual_scroll_x = 0;
                if (manual_scroll_y < 0) manual_scroll_y = 0;
                if (manual_scroll_x > max_x) manual_scroll_x = max_x;
                if (manual_scroll_y > max_y) manual_scroll_y = max_y;
            }
        }
        
        last_key = current_key;
    }
    
    void update_auto_scroll() {
        float time = frame_count * 0.02f; // Smooth time progression
        
        // Large circular motion to demonstrate unlimited scrolling
        float radius_x = (WORLD_WIDTH / 3.0f) * TILE_SIZE;  // 1/3 of world width
        float radius_y = (WORLD_HEIGHT / 3.0f) * TILE_SIZE; // 1/3 of world height
        
        manual_scroll_x = world_center_x + sin(time * 0.3f) * radius_x;
        manual_scroll_y = world_center_y + cos(time * 0.2f) * radius_y;
        
        // Ensure we stay within world bounds
        float max_x = (WORLD_WIDTH - 1) * TILE_SIZE;
        float max_y = (WORLD_HEIGHT - 1) * TILE_SIZE;
        
        manual_scroll_x = fmax(0, fmin(max_x, manual_scroll_x));
        manual_scroll_y = fmax(0, fmin(max_y, manual_scroll_y));
    }
    
    void update_display() {
        // Set up display colors and background
        set_background_color(12, 12, 38); // Dark blue
        clear_text();
        
        // Title and basic info
        set_text_ink(255, 255, 0, 255); // Yellow
        print_at(2, 1, "Unlimited Scrolling Demo");
        
        set_text_ink(204, 204, 204, 255); // Light gray
        print_at(2, 3, "Controls:");
        print_at(4, 4, "SPACE - Toggle auto-scroll");
        print_at(4, 5, "TAB   - Toggle debug info");
        print_at(4, 6, "Arrow Keys - Manual scroll (when auto off)");
        print_at(4, 7, "SHIFT + Arrow - Fast scroll");
        print_at(4, 8, "ESC   - Exit demo");
        
        // Current mode indicator with more visibility
        set_text_ink(auto_scroll ? 0 : 255, auto_scroll ? 255 : 204, 0, 255);
        print_at(2, 10, auto_scroll ? "Mode: AUTO-SCROLL (Press SPACE to control manually)" : "Mode: MANUAL CONTROL (Use arrow keys)");
        
        if (!auto_scroll) {
            set_text_ink(255, 255, 0, 255); // Bright yellow for manual mode
            print_at(2, 11, ">>> USE ARROW KEYS TO SCROLL <<<");
        }
        
        if (show_debug) {
            display_debug_info();
        }
        
        // Input status indicator using is_key_pressed() which works for arrow keys
        set_text_ink(255, 200, 100, 255); // Orange
        print_at(2, 20, "Input Status:");
        set_text_ink(200, 200, 200, 255);
        print_at(15, 20, is_key_pressed(INPUT_KEY_LEFT) ? "[LEFT]" : "     ");
        print_at(22, 20, is_key_pressed(INPUT_KEY_RIGHT) ? "[RIGHT]" : "      ");
        print_at(29, 20, is_key_pressed(INPUT_KEY_UP) ? "[UP]" : "    ");
        print_at(34, 20, is_key_pressed(INPUT_KEY_DOWN) ? "[DOWN]" : "      ");
        
        // Instructions at bottom
        set_text_ink(153, 204, 255, 255); // Light blue
        print_at(2, 22, "Watch the tile patterns move to see unlimited scrolling!");
        print_at(2, 23, "Notice viewport shifting at tile boundaries (grid lines)");
        
        char progress[60];
        snprintf(progress, sizeof(progress), "Frame: %d", frame_count);
        print_at(2, 25, progress);
    }
    
    void display_debug_info() {
        set_text_ink(0, 255, 255, 255); // Cyan
        print_at(30, 3, "Debug Info:");
        
        // Current scroll position
        char scroll_info[80];
        snprintf(scroll_info, sizeof(scroll_info), "Scroll: (%.1f, %.1f)", 
                manual_scroll_x, manual_scroll_y);
        print_at(30, 5, scroll_info);
        
        // Current tile position
        int tile_x = (int)(manual_scroll_x / TILE_SIZE);
        int tile_y = (int)(manual_scroll_y / TILE_SIZE);
        char tile_info[80];
        snprintf(tile_info, sizeof(tile_info), "Tile: (%d, %d)", tile_x, tile_y);
        print_at(30, 6, tile_info);
        
        // Sub-tile position
        float sub_x = fmod(manual_scroll_x, TILE_SIZE);
        float sub_y = fmod(manual_scroll_y, TILE_SIZE);
        char sub_info[80];
        snprintf(sub_info, sizeof(sub_info), "Sub-tile: (%.1f, %.1f)", sub_x, sub_y);
        print_at(30, 7, sub_info);
        
        // World info
        char world_info[80];
        snprintf(world_info, sizeof(world_info), "World: %dx%d tiles", WORLD_WIDTH, WORLD_HEIGHT);
        print_at(30, 9, world_info);
        
        // Viewport shifting indicator
        bool near_x_boundary = (sub_x < 10.0f || sub_x > TILE_SIZE - 10.0f);
        bool near_y_boundary = (sub_y < 10.0f || sub_y > TILE_SIZE - 10.0f);
        
        if (near_x_boundary || near_y_boundary) {
            set_text_ink(255, 204, 0, 255); // Orange
            print_at(30, 11, ">>> VIEWPORT SHIFTING <<<");
        } else {
            set_text_ink(0, 255, 0, 255); // Green
            print_at(30, 11, "Smooth sub-tile scroll");
        }
        
        // Performance info
        set_text_ink(204, 204, 204, 255);
        char perf_info[80];
        float seconds = frame_count * 0.02f;
        snprintf(perf_info, sizeof(perf_info), "Time: %.1fs", seconds);
        print_at(30, 13, perf_info);
    }
    
public:
    bool should_continue() {
        return !should_quit();
    }
};

// Main demo function that will be called by the runtime
void run_unlimited_scroll_demo() {
    console_section("Starting Unlimited Scrolling Demo");
    console_info("This demo showcases unlimited tile-based scrolling");
    console_info("with viewport shifting and performance optimization");
    console_separator();
    
    UnlimitedScrollDemo demo;
    
    // Set up environment
    set_background_color(12, 12, 38);
    clear_graphics();
    clear_text();
    
    // Display loading screen
    set_text_ink(255, 255, 0, 255);
    print_at(15, 8, "Unlimited Scrolling Demo");
    print_at(20, 10, "Initializing...");
    
    set_text_ink(255, 255, 255, 255);
    print_at(10, 12, "Console output shows detailed initialization progress");
    print_at(10, 13, "Check terminal for debugging information!");
    
    // Initialize demo (runtime systems are already initialized)
    demo.initialize();
    
    console_section("Demo Running");
    console_info("Controls: SPACE=toggle auto-scroll, TAB=toggle debug, ESC=exit");
    console_info("Starting main demo loop...");
    console_info("Press SPACE to toggle auto-scroll, TAB for debug, ESC to exit");
    console_info("Watch the Input Status indicators in the app to see key presses");
    
    // Main demo loop
    int loop_count = 0;
    while (demo.should_continue()) {
        demo.update();
        
        // Log progress every 300 frames (about 6 seconds at 50fps)
        if (++loop_count % 300 == 0) {
            console_printf("Demo running... %d frames processed", loop_count);
        }
        
        // Frame rate control (approximately 50 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    console_section("Demo Cleanup");
    console_info("User requested exit, shutting down demo");
    
    // Cleanup display
    clear_text();
    set_text_ink(0, 255, 0, 255);
    print_at(25, 12, "Demo Complete!");
    print_at(20, 14, "Thank you for testing unlimited scrolling!");
    
    console_info("Demo completed successfully!");
    console_printf("Total frames processed: %d", loop_count);
    
    // Brief pause before exit
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

int main() {
    console_section("Abstract Runtime - Unlimited Scrolling Demo");
    console_info("Modern C++ demo showcasing unlimited tile-based scrolling");
    
    // Use run_runtime_with_app for proper initialization and main loop
    return run_runtime_with_app(SCREEN_800x600, run_unlimited_scroll_demo);
}