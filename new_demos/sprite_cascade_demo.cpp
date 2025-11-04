#include "abstract_runtime.h"
#include "input_system.h"
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <random>
#include <cmath>

/**
 * Pure Graphics Sprite Cascade Demo
 * 
 * A modern C++ application that demonstrates:
 * - Pure graphics rendering (no console I/O)
 * - Object-oriented particle system
 * - Real-time physics simulation
 * - Interactive controls via keyboard
 */

namespace SpriteCascade {

    // Configuration
    constexpr int MAX_SPRITES = 128;
    constexpr int MAX_PARTICLES = 256;
    constexpr float GRAVITY = 150.0f;
    constexpr float AIR_RESISTANCE = 0.998f;
    constexpr float BOUNCE_DAMPING = 0.7f;
    constexpr int MAX_BOUNCES = 4;
    constexpr float FADE_START_TIME = 12.0f;
    constexpr float FADE_RATE = 0.5f;
    constexpr int TARGET_FPS = 60;
    constexpr float DELTA_TIME = 1.0f / TARGET_FPS;

    // Random number generator
    class RandomGenerator {
    private:
        std::random_device rd_;
        std::mt19937 gen_;
        std::uniform_real_distribution<float> float_dist_;

    public:
        RandomGenerator() : gen_(rd_()), float_dist_(0.0f, 1.0f) {}

        float random_float(float min = 0.0f, float max = 1.0f) {
            return min + (max - min) * float_dist_(gen_);
        }

        int random_int(int min, int max) {
            std::uniform_int_distribution<int> dist(min, max);
            return dist(gen_);
        }

        float random_angle() {
            return random_float(0.0f, 2.0f * static_cast<float>(M_PI));
        }
    };

    // 2D Vector
    struct Vector2 {
        float x, y;

        Vector2(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}

        Vector2 operator+(const Vector2& other) const {
            return {x + other.x, y + other.y};
        }

        Vector2 operator*(float scalar) const {
            return {x * scalar, y * scalar};
        }

        Vector2& operator+=(const Vector2& other) {
            x += other.x;
            y += other.y;
            return *this;
        }

        Vector2& operator*=(float scalar) {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        static Vector2 from_angle(float angle, float magnitude = 1.0f) {
            return {std::cos(angle) * magnitude, std::sin(angle) * magnitude};
        }
    };

    // Particle representing a single sprite
    class Particle {
    private:
        Vector2 position_;
        Vector2 velocity_;
        Vector2 acceleration_;
        float rotation_;
        float rotation_speed_;
        float scale_;
        float alpha_;
        float life_time_;
        int bounce_count_;
        int sprite_slot_;
        int instance_id_;
        bool active_;

        static RandomGenerator& get_random() {
            static RandomGenerator rng;
            return rng;
        }

    public:
        Particle(int instance_id, int sprite_slot, const Vector2& start_pos)
            : position_(start_pos)
            , velocity_(0.0f, 0.0f)
            , acceleration_(0.0f, GRAVITY)
            , rotation_(0.0f)
            , rotation_speed_(get_random().random_float(-300.0f, 300.0f))
            , scale_(get_random().random_float(0.4f, 1.2f))
            , alpha_(1.0f)
            , life_time_(0.0f)
            , bounce_count_(0)
            , sprite_slot_(sprite_slot)
            , instance_id_(instance_id)
            , active_(false)
        {
            // Create burst pattern
            float angle = get_random().random_angle();
            float speed = get_random().random_float(100.0f, 200.0f);
            velocity_ = Vector2::from_angle(angle, speed);
        }

        void activate() {
            active_ = true;
            life_time_ = 0.0f;
            
            sprite(instance_id_, sprite_slot_, 
                   static_cast<int>(position_.x), static_cast<int>(position_.y));
            sprite_scale(instance_id_, scale_, scale_);
            sprite_rotate(instance_id_, rotation_);
            sprite_alpha(instance_id_, alpha_);
        }

        void update(float dt, int screen_width, int screen_height) {
            if (!active_) return;

            life_time_ += dt;

            // Physics
            velocity_ += acceleration_ * dt;
            position_ += velocity_ * dt;
            velocity_ *= AIR_RESISTANCE;

            // Ground collision
            const float ground_y = static_cast<float>(screen_height - 32);
            if (position_.y > ground_y && velocity_.y > 0) {
                if (bounce_count_ < MAX_BOUNCES) {
                    velocity_.y = -velocity_.y * BOUNCE_DAMPING;
                    position_.y = ground_y;
                    bounce_count_++;
                    velocity_.x += get_random().random_float(-50.0f, 50.0f);
                    rotation_speed_ *= 1.3f;
                }
            }

            // Side walls
            if (position_.x < 0 && velocity_.x < 0) {
                velocity_.x = -velocity_.x * 0.8f;
                position_.x = 0;
            } else if (position_.x > screen_width && velocity_.x > 0) {
                velocity_.x = -velocity_.x * 0.8f;
                position_.x = static_cast<float>(screen_width);
            }

            // Rotation
            rotation_ += rotation_speed_ * dt;

            // Fade out
            if (life_time_ > FADE_START_TIME) {
                float fade_progress = (life_time_ - FADE_START_TIME) * FADE_RATE;
                alpha_ = std::max(0.0f, 1.0f - fade_progress);
            }

            // Deactivate if needed
            if (alpha_ <= 0.0f || 
                (position_.y > screen_height + 200 && bounce_count_ >= MAX_BOUNCES) ||
                position_.x < -200 || position_.x > screen_width + 200) {
                deactivate();
                return;
            }

            // Update sprite
            sprite_move(instance_id_, static_cast<int>(position_.x), static_cast<int>(position_.y));
            sprite_rotate(instance_id_, rotation_);
            sprite_alpha(instance_id_, alpha_);
        }

        void deactivate() {
            active_ = false;
            sprite_hide(instance_id_);
        }

        bool is_active() const { return active_; }
        int get_bounce_count() const { return bounce_count_; }
    };

    // Main demo state
    class SpriteCascadeDemo {
    private:
        std::vector<std::unique_ptr<Particle>> particles_;
        int screen_width_;
        int screen_height_;
        int sprites_loaded_;
        int spawn_counter_;
        int spawn_rate_;
        float spawn_timer_;
        bool demo_running_;
        bool paused_;
        bool loading_complete_;
        float loading_progress_;

    public:
        SpriteCascadeDemo() 
            : screen_width_(0), screen_height_(0), sprites_loaded_(0)
            , spawn_counter_(0), spawn_rate_(3), spawn_timer_(0.0f)
            , demo_running_(true), paused_(false)
            , loading_complete_(false), loading_progress_(0.0f)
        {
            particles_.reserve(MAX_PARTICLES);
        }

        void initialize() {
            screen_width_ = get_screen_width();
            screen_height_ = get_screen_height();
            
            // Dark space background
            set_background_color(10, 15, 30);
            clear_graphics();
            
            // Initialize input system for keyboard controls
            if (!init_input_system()) {
                demo_running_ = false;
                return;
            }
            
            // Initialize sprite system
            if (!init_sprites()) {
                demo_running_ = false;
                return;
            }

            // Load sprites with visual progress
            load_sprites();
            
            if (sprites_loaded_ == 0) {
                demo_running_ = false;
                return;
            }

            // Create particle pool
            create_particles();
            loading_complete_ = true;
        }

        void run() {
            initialize();
            
            if (!demo_running_) return;

            disable_auto_quit();
            
            const auto frame_duration = std::chrono::milliseconds(1000 / TARGET_FPS);

            while (demo_running_ && !should_quit()) {
                auto frame_start = std::chrono::high_resolution_clock::now();
                
                handle_input();
                
                if (loading_complete_ && !paused_) {
                    update(DELTA_TIME);
                }
                
                render();

                // Frame limiting
                auto frame_end = std::chrono::high_resolution_clock::now();
                auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
                if (frame_time < frame_duration) {
                    std::this_thread::sleep_for(frame_duration - frame_time);
                }
            }

            cleanup();
        }

    private:
        void load_sprites() {
            sprites_loaded_ = 0;
            
            for (int slot = 0; slot < MAX_SPRITES; ++slot) {
                char filename[64];
                snprintf(filename, sizeof(filename), "../assets/sprite%03d.png", slot + 1);
                
                if (load_sprite(slot, filename)) {
                    ++sprites_loaded_;
                }
                
                // Update loading progress visually
                loading_progress_ = static_cast<float>(slot + 1) / MAX_SPRITES;
                
                // Draw loading bar
                clear_graphics();
                set_draw_color(100, 100, 100, 255);
                draw_rect(screen_width_ / 4, screen_height_ / 2 - 10, screen_width_ / 2, 20);
                
                set_draw_color(50, 150, 50, 255);
                int bar_width = static_cast<int>((screen_width_ / 2) * loading_progress_);
                draw_rect(screen_width_ / 4, screen_height_ / 2 - 10, bar_width, 20);
                
                // Small delay to show progress
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        void create_particles() {
            Vector2 center(screen_width_ / 2.0f, screen_height_ / 2.0f - 100.0f);
            
            for (int i = 0; i < MAX_PARTICLES && i < sprites_loaded_; ++i) {
                int sprite_slot = i % sprites_loaded_;
                particles_.push_back(std::make_unique<Particle>(i, sprite_slot, center));
            }
        }

        void handle_input() {
            // Toggle pause
            if (is_key_pressed(INPUT_KEY_SPACE)) {
                paused_ = !paused_;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            // Adjust spawn rate (use ASCII values)
            if (is_key_pressed(61) || is_key_pressed(43)) { // '=' or '+'
                spawn_rate_ = std::min(spawn_rate_ + 1, 10);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (is_key_pressed(45)) { // '-'
                spawn_rate_ = std::max(spawn_rate_ - 1, 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Reset
            if (is_key_pressed(114) || is_key_pressed(82)) { // 'r' or 'R'
                reset_demo();
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            // Quit
            if (is_key_pressed(INPUT_KEY_ESCAPE)) {
                demo_running_ = false;
            }
        }

        void update(float dt) {
            spawn_timer_ += dt;

            // Spawn particles
            if (spawn_timer_ >= (1.0f / spawn_rate_) && spawn_counter_ < static_cast<int>(particles_.size())) {
                for (int i = 0; i < spawn_rate_ && spawn_counter_ < static_cast<int>(particles_.size()); ++i) {
                    particles_[spawn_counter_]->activate();
                    ++spawn_counter_;
                }
                spawn_timer_ = 0.0f;
            }

            // Update particles
            int active_count = 0;
            for (auto& particle : particles_) {
                particle->update(dt, screen_width_, screen_height_);
                if (particle->is_active()) {
                    ++active_count;
                }
            }

            // Auto-reset when all done
            if (spawn_counter_ >= static_cast<int>(particles_.size()) && active_count == 0) {
                reset_demo();
            }
        }

        void render() {
            if (!loading_complete_) return;

            // Draw status indicators using simple graphics
            set_draw_color(255, 255, 255, 200);
            
            // Active particle count indicator (dots at top)
            int active_count = 0;
            for (const auto& particle : particles_) {
                if (particle->is_active()) {
                    ++active_count;
                }
            }
            
            // Draw activity indicator
            for (int i = 0; i < active_count / 4; ++i) {
                draw_circle(20 + i * 6, 20, 2);
            }

            // Spawn progress bar at bottom
            if (spawn_counter_ < static_cast<int>(particles_.size())) {
                float progress = static_cast<float>(spawn_counter_) / static_cast<float>(particles_.size());
                set_draw_color(50, 50, 150, 180);
                draw_rect(20, screen_height_ - 30, screen_width_ - 40, 10);
                
                set_draw_color(100, 100, 255, 200);
                int progress_width = static_cast<int>((screen_width_ - 40) * progress);
                draw_rect(20, screen_height_ - 30, progress_width, 10);
            }

            // Pause indicator
            if (paused_) {
                set_draw_color(255, 255, 0, 180);
                draw_rect(screen_width_ / 2 - 50, 50, 20, 40);
                draw_rect(screen_width_ / 2 + 30, 50, 20, 40);
            }
        }

        void reset_demo() {
            // Hide all sprites
            for (auto& particle : particles_) {
                particle->deactivate();
            }

            spawn_counter_ = 0;
            spawn_timer_ = 0.0f;
            create_particles();
        }

        void cleanup() {
            enable_auto_quit();
            hide_all_sprites();
        }
    };

} // namespace SpriteCascade

// Application entry point - this is what gets called by run_runtime_with_app
void sprite_cascade_demo() {
    SpriteCascade::SpriteCascadeDemo demo;
    demo.run();
}

int main() {
    return run_runtime_with_app(SCREEN_800x600, sprite_cascade_demo);
}