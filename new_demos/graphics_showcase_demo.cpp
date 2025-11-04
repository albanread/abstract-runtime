#include "abstract_runtime.h"
#include "input_system.h"
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <random>
#include <cmath>

/**
 * Graphics Showcase Demo
 * 
 * Demonstrates the runtime's graphics layer with:
 * - Fractal ferns using Barnsley's fern algorithm
 * - Particle-based fireworks system
 * - Interactive controls for switching between modes
 */

namespace GraphicsShowcase {

    // Configuration
    constexpr int TARGET_FPS = 60;
    constexpr float DELTA_TIME = 1.0f / TARGET_FPS;

    // Demo modes
    enum class DemoMode {
        FERNS,
        FIREWORKS,
        BOTH
    };

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
    };

    // 2D Point
    struct Point {
        float x, y;
        Point(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}
    };

    // Color structure
    struct Color {
        int r, g, b, a;
        Color(int r = 255, int g = 255, int b = 255, int a = 255) : r(r), g(g), b(b), a(a) {}
    };

    // Fractal Fern Generator using Barnsley's Fern
    class FractalFern {
    private:
        std::vector<Point> points_;
        std::vector<Color> colors_;
        RandomGenerator& rng_;
        int max_points_;
        int current_iteration_;
        Point current_point_;
        Color base_color_;
        float scale_;
        Point offset_;

    public:
        FractalFern(RandomGenerator& rng, const Color& base_color, int max_points = 50000)
            : rng_(rng), max_points_(max_points), current_iteration_(0)
            , current_point_(0.0f, 0.0f), base_color_(base_color)
            , scale_(50.0f), offset_(400.0f, 500.0f)
        {
            points_.reserve(max_points);
            colors_.reserve(max_points);
        }

        void set_position(float x, float y, float scale) {
            offset_ = Point(x, y);
            scale_ = scale;
        }

        void update() {
            // Generate several points per frame for faster generation
            for (int i = 0; i < 100 && current_iteration_ < max_points_; ++i) {
                float r = rng_.random_float();
                Point next_point;

                // Barnsley's fern transformation probabilities
                if (r < 0.01f) {
                    // Stem (1% probability)
                    next_point.x = 0.0f;
                    next_point.y = 0.16f * current_point_.y;
                } else if (r < 0.86f) {
                    // Leaflet (85% probability)
                    next_point.x = 0.85f * current_point_.x + 0.04f * current_point_.y;
                    next_point.y = -0.04f * current_point_.x + 0.85f * current_point_.y + 1.6f;
                } else if (r < 0.93f) {
                    // Left leaflet (7% probability)
                    next_point.x = 0.2f * current_point_.x - 0.26f * current_point_.y;
                    next_point.y = 0.23f * current_point_.x + 0.22f * current_point_.y + 1.6f;
                } else {
                    // Right leaflet (7% probability)
                    next_point.x = -0.15f * current_point_.x + 0.28f * current_point_.y;
                    next_point.y = 0.26f * current_point_.x + 0.24f * current_point_.y + 0.44f;
                }

                current_point_ = next_point;
                points_.push_back(current_point_);

                // Vary color based on y position for gradient effect
                float gradient = (current_point_.y + 2.0f) / 12.0f; // Normalize to 0-1
                gradient = std::max(0.0f, std::min(1.0f, gradient));
                
                Color point_color;
                point_color.r = static_cast<int>(base_color_.r * (0.3f + 0.7f * gradient));
                point_color.g = static_cast<int>(base_color_.g * (0.5f + 0.5f * gradient));
                point_color.b = static_cast<int>(base_color_.b * (0.2f + 0.8f * gradient));
                point_color.a = base_color_.a;
                
                colors_.push_back(point_color);
                current_iteration_++;
            }
        }

        void render() {
            for (size_t i = 0; i < points_.size(); ++i) {
                const Point& p = points_[i];
                const Color& c = colors_[i];
                
                // Transform to screen coordinates
                int screen_x = static_cast<int>(offset_.x + p.x * scale_);
                int screen_y = static_cast<int>(offset_.y - p.y * scale_); // Flip Y
                
                set_draw_color(c.r, c.g, c.b, c.a);
                draw_circle(screen_x, screen_y, 1);
            }
        }

        void reset() {
            points_.clear();
            colors_.clear();
            current_iteration_ = 0;
            current_point_ = Point(0.0f, 0.0f);
        }

        bool is_complete() const {
            return current_iteration_ >= max_points_;
        }

        int get_point_count() const {
            return static_cast<int>(points_.size());
        }
    };

    // Firework particle
    class FireworkParticle {
    private:
        Point position_;
        Point velocity_;
        Point acceleration_;
        Color color_;
        float life_time_;
        float max_life_;
        bool active_;
        float alpha_;

    public:
        FireworkParticle(const Point& pos, const Point& vel, const Color& color, float life)
            : position_(pos), velocity_(vel), acceleration_(0.0f, 100.0f) // Gravity
            , color_(color), life_time_(0.0f), max_life_(life), active_(true), alpha_(1.0f)
        {}

        void update(float dt) {
            if (!active_) return;

            life_time_ += dt;
            
            // Physics
            velocity_.x += acceleration_.x * dt;
            velocity_.y += acceleration_.y * dt;
            
            position_.x += velocity_.x * dt;
            position_.y += velocity_.y * dt;

            // Air resistance
            velocity_.x *= 0.998f;
            velocity_.y *= 0.998f;

            // Fade out over time
            alpha_ = 1.0f - (life_time_ / max_life_);
            if (alpha_ <= 0.0f || life_time_ >= max_life_) {
                active_ = false;
            }
        }

        void render() {
            if (!active_) return;

            Color render_color = color_;
            render_color.a = static_cast<int>(255 * alpha_);
            
            set_draw_color(render_color.r, render_color.g, render_color.b, render_color.a);
            draw_circle(static_cast<int>(position_.x), static_cast<int>(position_.y), 2);
        }

        bool is_active() const { return active_; }
        const Point& get_position() const { return position_; }
    };

    // Firework explosion
    class Firework {
    private:
        std::vector<std::unique_ptr<FireworkParticle>> particles_;
        Point center_;
        Color color_;
        bool exploded_;
        RandomGenerator& rng_;

    public:
        Firework(RandomGenerator& rng, const Point& center, const Color& color)
            : center_(center), color_(color), exploded_(false), rng_(rng)
        {}

        void explode() {
            if (exploded_) return;

            // Create burst of particles
            int particle_count = rng_.random_int(30, 80);
            
            for (int i = 0; i < particle_count; ++i) {
                float angle = rng_.random_float(0.0f, 2.0f * M_PI);
                float speed = rng_.random_float(50.0f, 200.0f);
                
                Point velocity;
                velocity.x = std::cos(angle) * speed;
                velocity.y = std::sin(angle) * speed;
                
                // Vary particle colors slightly
                Color particle_color = color_;
                particle_color.r = std::max(0, std::min(255, particle_color.r + rng_.random_int(-30, 30)));
                particle_color.g = std::max(0, std::min(255, particle_color.g + rng_.random_int(-30, 30)));
                particle_color.b = std::max(0, std::min(255, particle_color.b + rng_.random_int(-30, 30)));
                
                float life = rng_.random_float(1.0f, 3.0f);
                particles_.push_back(std::make_unique<FireworkParticle>(center_, velocity, particle_color, life));
            }
            
            exploded_ = true;
        }

        void update(float dt) {
            for (auto& particle : particles_) {
                particle->update(dt);
            }
            
            // Remove inactive particles
            particles_.erase(
                std::remove_if(particles_.begin(), particles_.end(),
                    [](const std::unique_ptr<FireworkParticle>& p) {
                        return !p->is_active();
                    }),
                particles_.end()
            );
        }

        void render() {
            for (auto& particle : particles_) {
                particle->render();
            }
        }

        bool is_finished() const {
            return exploded_ && particles_.empty();
        }

        void force_explode() {
            if (!exploded_) {
                explode();
            }
        }
    };

    // Main demo controller
    class GraphicsShowcaseDemo {
    private:
        RandomGenerator rng_;
        DemoMode current_mode_;
        int screen_width_;
        int screen_height_;
        bool demo_running_;
        
        // Ferns
        std::vector<std::unique_ptr<FractalFern>> ferns_;
        
        // Fireworks
        std::vector<std::unique_ptr<Firework>> fireworks_;
        float firework_timer_;
        float firework_interval_;
        
        // Background
        float background_hue_;

    public:
        GraphicsShowcaseDemo()
            : current_mode_(DemoMode::FERNS)
            , screen_width_(0), screen_height_(0)
            , demo_running_(true), firework_timer_(0.0f)
            , firework_interval_(1.0f), background_hue_(0.0f)
        {}

        void initialize() {
            screen_width_ = get_screen_width();
            screen_height_ = get_screen_height();
            
            // Initialize input system
            if (!init_input_system()) {
                demo_running_ = false;
                return;
            }
            
            // Create multiple ferns with different colors and positions
            create_ferns();
        }

        void create_ferns() {
            ferns_.clear();
            
            // Create 3 ferns with different shades of green
            Color green1(50, 200, 50);
            Color green2(80, 150, 80);
            Color green3(30, 180, 30);
            
            auto fern1 = std::make_unique<FractalFern>(rng_, green1, 30000);
            fern1->set_position(screen_width_ * 0.25f, screen_height_ * 0.8f, 40.0f);
            ferns_.push_back(std::move(fern1));
            
            auto fern2 = std::make_unique<FractalFern>(rng_, green2, 35000);
            fern2->set_position(screen_width_ * 0.5f, screen_height_ * 0.85f, 50.0f);
            ferns_.push_back(std::move(fern2));
            
            auto fern3 = std::make_unique<FractalFern>(rng_, green3, 25000);
            fern3->set_position(screen_width_ * 0.75f, screen_height_ * 0.8f, 35.0f);
            ferns_.push_back(std::move(fern3));
        }

        void run() {
            initialize();
            
            if (!demo_running_) return;

            disable_auto_quit();
            
            const auto frame_duration = std::chrono::milliseconds(1000 / TARGET_FPS);

            while (demo_running_ && !should_quit()) {
                auto frame_start = std::chrono::high_resolution_clock::now();
                
                handle_input();
                update(DELTA_TIME);
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
        void handle_input() {
            // Mode switching
            if (is_key_pressed(49)) { // '1'
                if (current_mode_ != DemoMode::FERNS) {
                    current_mode_ = DemoMode::FERNS;
                    reset_ferns();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            if (is_key_pressed(50)) { // '2'
                current_mode_ = DemoMode::FIREWORKS;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            if (is_key_pressed(51)) { // '3'
                current_mode_ = DemoMode::BOTH;
                if (ferns_.empty() || ferns_[0]->is_complete()) {
                    reset_ferns();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            // Manual firework launch
            if (is_key_pressed(INPUT_KEY_SPACE)) {
                launch_firework();
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
            // Update background hue for subtle color cycling
            background_hue_ += dt * 10.0f;
            if (background_hue_ > 360.0f) background_hue_ -= 360.0f;

            // Update ferns
            if (current_mode_ == DemoMode::FERNS || current_mode_ == DemoMode::BOTH) {
                for (auto& fern : ferns_) {
                    if (!fern->is_complete()) {
                        fern->update();
                    }
                }
            }

            // Update fireworks
            if (current_mode_ == DemoMode::FIREWORKS || current_mode_ == DemoMode::BOTH) {
                firework_timer_ += dt;
                
                // Auto-launch fireworks
                if (firework_timer_ >= firework_interval_) {
                    launch_firework();
                    firework_timer_ = 0.0f;
                    firework_interval_ = rng_.random_float(0.5f, 2.0f);
                }

                // Update existing fireworks
                for (auto& firework : fireworks_) {
                    firework->update(dt);
                }

                // Remove finished fireworks
                fireworks_.erase(
                    std::remove_if(fireworks_.begin(), fireworks_.end(),
                        [](const std::unique_ptr<Firework>& f) {
                            return f->is_finished();
                        }),
                    fireworks_.end()
                );
            }
        }

        void render() {
            // Dynamic background based on mode
            if (current_mode_ == DemoMode::FERNS) {
                set_background_color(5, 15, 5); // Dark green
            } else if (current_mode_ == DemoMode::FIREWORKS) {
                set_background_color(5, 5, 20); // Dark blue
            } else {
                // Subtle color cycling for both mode
                int r = static_cast<int>(10 + 5 * std::sin(background_hue_ * M_PI / 180.0f));
                int g = static_cast<int>(10 + 5 * std::sin((background_hue_ + 120) * M_PI / 180.0f));
                int b = static_cast<int>(15 + 10 * std::sin((background_hue_ + 240) * M_PI / 180.0f));
                set_background_color(r, g, b);
            }

            clear_graphics();

            // Render ferns
            if (current_mode_ == DemoMode::FERNS || current_mode_ == DemoMode::BOTH) {
                for (auto& fern : ferns_) {
                    fern->render();
                }
            }

            // Render fireworks
            if (current_mode_ == DemoMode::FIREWORKS || current_mode_ == DemoMode::BOTH) {
                for (auto& firework : fireworks_) {
                    firework->render();
                }
            }

            // Draw UI indicators
            draw_ui();
        }

        void draw_ui() {
            // Mode indicators using simple graphics
            set_draw_color(255, 255, 255, 180);
            
            // Mode 1: Ferns
            if (current_mode_ == DemoMode::FERNS) {
                set_draw_color(0, 255, 0, 200);
            }
            draw_circle(30, 30, 8);
            
            // Mode 2: Fireworks  
            set_draw_color(255, 255, 255, 180);
            if (current_mode_ == DemoMode::FIREWORKS) {
                set_draw_color(255, 100, 0, 200);
            }
            draw_circle(60, 30, 8);
            
            // Mode 3: Both
            set_draw_color(255, 255, 255, 180);
            if (current_mode_ == DemoMode::BOTH) {
                set_draw_color(255, 255, 0, 200);
            }
            draw_circle(90, 30, 8);

            // Progress indicator for ferns
            if (current_mode_ == DemoMode::FERNS || current_mode_ == DemoMode::BOTH) {
                if (!ferns_.empty()) {
                    float total_progress = 0.0f;
                    for (const auto& fern : ferns_) {
                        total_progress += static_cast<float>(fern->get_point_count()) / 30000.0f;
                    }
                    total_progress /= ferns_.size();
                    total_progress = std::min(1.0f, total_progress);

                    set_draw_color(100, 100, 100, 100);
                    draw_rect(screen_width_ - 120, 20, 100, 8);
                    
                    set_draw_color(0, 255, 0, 150);
                    int progress_width = static_cast<int>(100 * total_progress);
                    draw_rect(screen_width_ - 120, 20, progress_width, 8);
                }
            }
        }

        void launch_firework() {
            Point launch_pos;
            launch_pos.x = rng_.random_float(screen_width_ * 0.1f, screen_width_ * 0.9f);
            launch_pos.y = rng_.random_float(screen_height_ * 0.2f, screen_height_ * 0.6f);
            
            // Random bright colors
            Color colors[] = {
                Color(255, 100, 100), // Red
                Color(100, 255, 100), // Green
                Color(100, 100, 255), // Blue
                Color(255, 255, 100), // Yellow
                Color(255, 100, 255), // Magenta
                Color(100, 255, 255)  // Cyan
            };
            
            Color firework_color = colors[rng_.random_int(0, 5)];
            
            auto firework = std::make_unique<Firework>(rng_, launch_pos, firework_color);
            firework->force_explode(); // Explode immediately
            fireworks_.push_back(std::move(firework));
        }

        void reset_ferns() {
            for (auto& fern : ferns_) {
                fern->reset();
            }
        }

        void reset_demo() {
            reset_ferns();
            fireworks_.clear();
            firework_timer_ = 0.0f;
            background_hue_ = 0.0f;
        }

        void cleanup() {
            enable_auto_quit();
            ferns_.clear();
            fireworks_.clear();
        }
    };

} // namespace GraphicsShowcase

// Application entry point
void graphics_showcase_demo() {
    GraphicsShowcase::GraphicsShowcaseDemo demo;
    demo.run();
}

int main() {
    return run_runtime_with_app(SCREEN_800x600, graphics_showcase_demo);
}