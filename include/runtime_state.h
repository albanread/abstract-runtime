#ifndef RUNTIME_STATE_H
#define RUNTIME_STATE_H

#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <string>
#include "abstract_runtime.h"
#include "sprite_bank.h"
#include "font_atlas.h"

// Enhanced input system constants
constexpr int MAX_INPUT_KEYS = 512;
constexpr int MAX_JOYSTICKS = 4;
constexpr int MAX_JOYSTICK_AXES = 8;
constexpr int MAX_JOYSTICK_BUTTONS = 32;

namespace AbstractRuntime {

// Forward declarations (already included above)
// class SpriteBank;
// class FontAtlas;

// =============================================================================
// CORE DATA STRUCTURES
// =============================================================================

/**
 * Sprite position data (atomic for lock-free updates)
 */
struct SpritePosition {
    int sprite_slot;
    int x, y;
    bool visible;
    float scale = 1.0f;
    float rotation = 0.0f;
    float alpha = 1.0f;
};

/**
 * Layer viewport offset
 */
struct ViewportOffset {
    int x = 0;
    int y = 0;
};

/**
 * Background color
 */
struct BackgroundColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

/**
 * Text grid data
 */
struct TextData {
    int cols = 0;
    int rows = 0;
    int cursor_x = 0;
    int cursor_y = 0;
    std::vector<char> characters;
    std::vector<uint32_t> colors;
    uint32_t current_color = COLOR_WHITE;
};

/**
 * Tile map data
 */
struct TileData {
    int cols = 0;
    int rows = 0;
    std::vector<int> tiles;  // -1 = empty, 0-255 = sprite slot
};

/**
 * Graphics command for vector drawing
 */
enum GraphicsCommandType {
    DRAW_LINE_CMD,
    DRAW_RECT_CMD,
    DRAW_CIRCLE_CMD,
    DRAW_ELLIPSE_CMD,
    FILL_RECT_CMD,
    FILL_CIRCLE_CMD,
    FILL_ELLIPSE_CMD,
    PLOT_POINT_CMD
};

struct GraphicsCommand {
    GraphicsCommandType type;
    int params[4];  // x1, y1, x2, y2 or x, y, width, height etc.
    uint32_t color;
    int line_width = 1;
};

/**
 * Layer types
 */
enum LayerType {
    LAYER_BACKGROUND = 0,
    LAYER_TILES2 = 1,
    LAYER_TILES1 = 2,
    LAYER_GRAPHICS = 3,
    LAYER_SPRITES = 4,
    LAYER_TEXT = 5
};

/**
 * Individual layer state
 */
struct Layer {
    LayerType type;
    bool visible = true;
    bool dirty = false;
    ViewportOffset viewport_offset;
    
    // Layer-specific data
    union {
        TextData text_data;
        TileData tile_data;
    };
    
    // Graphics commands queue
    std::vector<GraphicsCommand> graphics_commands;
    
    Layer() {
        // Initialize union to text_data by default
        new(&text_data) TextData();
    }
    
    ~Layer() {
        // Proper cleanup for union
        if (type == LAYER_TEXT) {
            text_data.~TextData();
        } else if (type == LAYER_TILES1 || type == LAYER_TILES2) {
            tile_data.~TileData();
        }
    }
};

/**
 * Input event for WAITKEY/INKEY queue
 */
enum InputEventType {
    INPUT_KEY_PRESS,
    INPUT_MOUSE_CLICK
};

struct InputEvent {
    InputEventType type;
    int keycode;
    uint64_t timestamp;
};

/**
 * Enhanced keyboard event structure for event-based input
 */
struct KeyEvent {
    enum Type {
        KEY_NONE = 0,
        KEY_DOWN,
        KEY_UP
    } type;
    
    int keycode;                 // Key code (INPUT_KEY_* constants)
    uint32_t modifiers;          // Modifier key state
    char text[32];               // UTF-8 text representation (for text input)
    uint64_t timestamp;          // Event timestamp
    
    KeyEvent() : type(KEY_NONE), keycode(0), modifiers(0), timestamp(0) {
        text[0] = '\0';
    }
};

/**
 * Enhanced mouse event structure for event-based input
 */
struct MouseEvent {
    enum Type {
        MOUSE_NONE = 0,
        MOUSE_BUTTON_DOWN,
        MOUSE_BUTTON_UP,
        MOUSE_MOTION,
        MOUSE_WHEEL
    } type;
    
    int x, y;                    // Mouse position
    int button;                  // Button number (1=left, 2=middle, 3=right)
    int wheel_x, wheel_y;        // Wheel scroll amounts
    uint32_t modifiers;          // Modifier key state
    uint64_t timestamp;          // Event timestamp
    
    MouseEvent() : type(MOUSE_NONE), x(0), y(0), button(0), wheel_x(0), wheel_y(0), modifiers(0), timestamp(0) {}
};

/**
 * Lock-free queue for input events
 */
template<typename T>
class LockFreeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;

public:
    void enqueue(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
    
    bool dequeue(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};

/**
 * Asset loading command
 */
enum AssetCommandType {
    LOAD_SPRITE_FILE,
    LOAD_SPRITE_SHEET
};

struct AssetLoadCommand {
    AssetCommandType type;
    int slot;
    std::string filename;
    int grid_w = 1;
    int grid_h = 1;
};

/**
 * Scientific graphing bounds
 */
struct GraphBounds {
    float min_x = -10.0f;
    float max_x = 10.0f;
    float min_y = -10.0f;
    float max_y = 10.0f;
};

/**
 * Graph scaling factors (computed from bounds)
 */
struct GraphScale {
    float x = 1.0f;
    float y = 1.0f;
};

/**
 * Performance statistics
 */
struct PerformanceStats {
    float fps = 60.0f;
    float avg_frame_time_ms = 16.67f;
    uint64_t frame_count = 0;
    uint64_t total_frames = 0;
};

// SpriteInstance is defined in sprite_renderer.h to avoid duplication

/**
 * Text input state
 */
struct TextInputState {
    bool cursor_visible = true;
    int cursor_blink_rate_ms = 500;
    bool echo_enabled = true;
    uint32_t input_color = COLOR_YELLOW;
    uint32_t cursor_color = COLOR_GREEN;
    
    // Current input line state
    std::string current_input;
    int cursor_position = 0;
    int input_x = 0;
    int input_y = 0;
    bool input_active = false;
};

// =============================================================================
// MAIN RUNTIME STATE
// =============================================================================

/**
 * Central runtime state - shared between user thread and compositor thread
 */
struct RuntimeState {
    // Screen configuration
    int screen_mode = SCREEN_40_COLUMN;
    int screen_width = 640;
    int screen_height = 480;
    
    // Layer system
    static constexpr int NUM_LAYERS = 6;
    Layer layers[NUM_LAYERS];
    
    // Sprite system (lock-free atomic updates)
    static constexpr int MAX_SPRITES = 128;
    std::atomic<SpritePosition> sprite_positions[MAX_SPRITES];
    
    // Viewport offsets (atomic for immediate updates)
    std::atomic<ViewportOffset> viewport_offsets[NUM_LAYERS];
    
    // Background settings
    std::atomic<BackgroundColor> background_color;
    std::atomic<int> background_sprite_slot{-1};
    
    // Input system (atomic state)
    static constexpr int MAX_KEYS = 512;
    std::atomic<bool> key_states[MAX_KEYS];
    std::atomic<uint32_t> mouse_buttons{0};
    std::atomic<int> mouse_x{0};
    std::atomic<int> mouse_y{0};
    std::atomic<uint32_t> mouse_clicked_flags{0};  // Reset each frame
    
    // Enhanced input system state
    struct EnhancedInputState {
        // Edge detection arrays (updated each frame)
        std::atomic<bool> key_pressed_this_frame[MAX_INPUT_KEYS];
        std::atomic<bool> key_released_this_frame[MAX_INPUT_KEYS];
        
        // Mouse wheel state
        std::atomic<int> mouse_wheel_x{0};
        std::atomic<int> mouse_wheel_y{0};
        
        // Mouse click edge detection
        std::atomic<uint32_t> mouse_clicked_this_frame{0};
        std::atomic<uint32_t> mouse_released_this_frame{0};
        
        // Modifier key state
        std::atomic<uint32_t> modifier_state{0};
        
        // Frame counter for edge detection
        std::atomic<uint64_t> current_frame{0};
        uint64_t last_key_frame[MAX_INPUT_KEYS];
        uint64_t last_mouse_frame;
        
        // Event queuing control
        std::atomic<bool> event_queuing_enabled{true};
        
        // Constructor to initialize arrays
        EnhancedInputState() {
            for (int i = 0; i < MAX_INPUT_KEYS; ++i) {
                key_pressed_this_frame[i].store(false, std::memory_order_relaxed);
                key_released_this_frame[i].store(false, std::memory_order_relaxed);
                last_key_frame[i] = 0;
            }
            last_mouse_frame = 0;
        }
    } enhanced_input;
    
    // Input queues
    LockFreeQueue<InputEvent> input_queue;
    LockFreeQueue<KeyEvent> key_event_queue;
    LockFreeQueue<MouseEvent> mouse_event_queue;
    TextInputState text_input;
    
    // Asset management queues
    LockFreeQueue<AssetLoadCommand> asset_queue;
    LockFreeQueue<GraphicsCommand> graphics_queue;
    
    // Resource management (mutex protected)
    std::mutex sprite_bank_mutex;
    std::unique_ptr<SpriteBank> sprite_bank;
    std::unique_ptr<FontAtlas> font_atlas;
    
    // Scientific graphing state
    GraphBounds graph_bounds;
    GraphScale graph_scale;
    std::atomic<uint32_t> current_draw_color{COLOR_WHITE};
    std::atomic<int> current_line_width{1};
    
    // Text system state
    std::atomic<uint32_t> current_text_color{COLOR_WHITE};
    std::atomic<int> text_cursor_x{0};
    std::atomic<int> text_cursor_y{0};
    
    // Performance monitoring
    PerformanceStats performance_stats;
    
    // Runtime flags
    std::atomic<bool> runtime_initialized{false};
    std::atomic<bool> shutdown_requested{false};
    
    RuntimeState() {
        // Initialize sprite positions as invisible
        for (int i = 0; i < MAX_SPRITES; ++i) {
            sprite_positions[i].store({0, 0, 0, false}, std::memory_order_relaxed);
        }
        
        // Initialize viewport offsets
        for (int i = 0; i < NUM_LAYERS; ++i) {
            viewport_offsets[i].store({0, 0}, std::memory_order_relaxed);
        }
        
        // Initialize key states
        for (int i = 0; i < MAX_KEYS; ++i) {
            key_states[i].store(false, std::memory_order_relaxed);
        }
        
        // Enhanced input state is initialized by its constructor
        
        // Set default background color
        background_color.store({0, 0, 0}, std::memory_order_relaxed);
        
        // Initialize graph scaling
        update_graph_scale();
    }
    
    /**
     * Update graph scaling factors when bounds change
     */
    void update_graph_scale() {
        float width_range = graph_bounds.max_x - graph_bounds.min_x;
        float height_range = graph_bounds.max_y - graph_bounds.min_y;
        
        if (width_range > 0 && height_range > 0) {
            graph_scale.x = screen_width / width_range;
            graph_scale.y = screen_height / height_range;
        }
    }
    
    /**
     * Transform world coordinates to screen coordinates for graphing
     */
    void world_to_screen(float world_x, float world_y, int& screen_x, int& screen_y) const {
        screen_x = static_cast<int>((world_x - graph_bounds.min_x) * graph_scale.x);
        screen_y = static_cast<int>(screen_height - (world_y - graph_bounds.min_y) * graph_scale.y);
    }
    
    /**
     * Transform screen coordinates to world coordinates for mouse input
     */
    void screen_to_world(int screen_x, int screen_y, float& world_x, float& world_y) const {
        world_x = graph_bounds.min_x + (screen_x / graph_scale.x);
        world_y = graph_bounds.max_y - ((screen_y) / graph_scale.y);
    }
};

} // namespace AbstractRuntime

#endif // RUNTIME_STATE_H