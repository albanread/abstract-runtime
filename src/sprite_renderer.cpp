#include "sprite_renderer.h"
#include "sprite_bank.h"
#include <OpenGL/gl.h>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace AbstractRuntime {

SpriteRenderer::SpriteRenderer()
    : initialized_(false)
    , sprite_bank_(nullptr)
    , screen_width_(800)
    , screen_height_(600)
    , active_count_(0) {
}

SpriteRenderer::~SpriteRenderer() {
    shutdown();
}

bool SpriteRenderer::initialize(SpriteBank* sprite_bank, int screen_width, int screen_height) {
    if (initialized_) {
        return true;
    }
    
    if (!sprite_bank) {
        std::cerr << "SpriteRenderer: sprite_bank cannot be null" << std::endl;
        return false;
    }
    
    sprite_bank_ = sprite_bank;
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    active_count_ = 0;
    
    // Initialize all instances as inactive
    for (int i = 0; i < MAX_INSTANCES; ++i) {
        instances_[i] = SpriteInstance();
    }
    
    initialized_ = true;
    std::cout << "SpriteRenderer initialized with " << MAX_INSTANCES << " instance slots" << std::endl;
    return true;
}

void SpriteRenderer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    // Clear all instances
    for (int i = 0; i < MAX_INSTANCES; ++i) {
        instances_[i] = SpriteInstance();
    }
    
    active_count_ = 0;
    sprite_bank_ = nullptr;
    initialized_ = false;
    
    std::cout << "SpriteRenderer shutdown complete" << std::endl;
}

bool SpriteRenderer::sprite(int instance_id, int sprite_slot, float x, float y) {
    if (!initialized_) {
        return false;
    }
    
    if (!is_valid_instance_id(instance_id)) {
        return false;
    }
    
    if (!sprite_bank_) {
        return false;
    }
    
    if (!sprite_bank_->is_occupied(sprite_slot)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    // If this instance wasn't active before, increment counter
    if (!instances_[instance_id].active) {
        active_count_++;
    }
    
    instances_[instance_id].active = true;
    instances_[instance_id].sprite_slot = sprite_slot;
    instances_[instance_id].x = x;
    instances_[instance_id].y = y;
    instances_[instance_id].scale_x = 1.0f;
    instances_[instance_id].scale_y = 1.0f;
    instances_[instance_id].rotation = 0.0f;
    instances_[instance_id].alpha = 1.0f;
    instances_[instance_id].z_order = 0;
    

    
    return true;
}

bool SpriteRenderer::sprite_move(int instance_id, float x, float y) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (!instances_[instance_id].active) {
        return false;
    }
    
    // Add bounds checking to prevent invalid coordinates
    if (std::isnan(x) || std::isnan(y) || std::isinf(x) || std::isinf(y)) {
        return false;
    }
    
    instances_[instance_id].x = x;
    instances_[instance_id].y = y;
    
    return true;
}

bool SpriteRenderer::sprite_scale(int instance_id, float scale_x, float scale_y) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (!instances_[instance_id].active) {
        return false;
    }
    
    // Add bounds checking for scale values
    if (std::isnan(scale_x) || std::isnan(scale_y) || std::isinf(scale_x) || std::isinf(scale_y) ||
        scale_x <= 0.0f || scale_y <= 0.0f) {
        return false;
    }
    
    instances_[instance_id].scale_x = scale_x;
    instances_[instance_id].scale_y = scale_y;
    
    return true;
}

bool SpriteRenderer::sprite_rotate(int instance_id, float degrees) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (!instances_[instance_id].active) {
        return false;
    }
    
    // Add bounds checking for rotation
    if (std::isnan(degrees) || std::isinf(degrees)) {
        return false;
    }
    
    instances_[instance_id].rotation = degrees;
    
    return true;
}

bool SpriteRenderer::sprite_alpha(int instance_id, float alpha) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (!instances_[instance_id].active) {
        return false;
    }
    
    // Clamp alpha to valid range
    instances_[instance_id].alpha = std::max(0.0f, std::min(1.0f, alpha));
    
    return true;
}

bool SpriteRenderer::sprite_z_order(int instance_id, int z_order) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (!instances_[instance_id].active) {
        return false;
    }
    
    instances_[instance_id].z_order = z_order;
    
    return true;
}

bool SpriteRenderer::sprite_hide(int instance_id) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    if (instances_[instance_id].active) {
        instances_[instance_id].active = false;
        active_count_--;
    }
    
    return true;
}

bool SpriteRenderer::sprite_show(int instance_id) {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    // Can only show if it has a valid sprite slot
    if (instances_[instance_id].sprite_slot >= 0 && 
        sprite_bank_ && sprite_bank_->is_occupied(instances_[instance_id].sprite_slot)) {
        
        if (!instances_[instance_id].active) {
            instances_[instance_id].active = true;
            active_count_++;
        }
        return true;
    }
    
    return false;
}

bool SpriteRenderer::sprite_is_visible(int instance_id) const {
    if (!initialized_ || !is_valid_instance_id(instance_id)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    return instances_[instance_id].active;
}

void SpriteRenderer::hide_all_sprites() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    for (int i = 0; i < MAX_INSTANCES; ++i) {
        instances_[i].active = false;
    }
    
    active_count_ = 0;
}

int SpriteRenderer::get_active_count() const {
    if (!initialized_) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    return active_count_;
}

void SpriteRenderer::render_sprites() {
    if (!initialized_ || !sprite_bank_ || active_count_ == 0) {
        return;
    }
    
    // Get sorted list of visible sprites
    std::vector<int> visible_sprites = get_sorted_visible_sprites();
    
    if (visible_sprites.empty()) {
        return;
    }
    
    // Set up OpenGL state once for all sprites
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    
    // Save current matrix state
    glPushMatrix();
    
    // Set up orthographic projection for 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, screen_width_, screen_height_, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Batch render sprites by texture to reduce texture binding
    GLuint current_texture = 0;
    for (int instance_id : visible_sprites) {
        const SpriteInstance& instance = instances_[instance_id];
        GLuint texture_id = sprite_bank_->get_texture(instance.sprite_slot);
        
        if (texture_id != current_texture) {
            glBindTexture(GL_TEXTURE_2D, texture_id);
            current_texture = texture_id;
        }
        
        render_sprite_instance_optimized(instance);
    }
    
    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    // Restore OpenGL state
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

void SpriteRenderer::update_screen_size(int width, int height) {
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    screen_width_ = width;
    screen_height_ = height;
}

bool SpriteRenderer::is_valid_instance_id(int instance_id) const {
    return instance_id >= 0 && instance_id < MAX_INSTANCES;
}

bool SpriteRenderer::is_sprite_on_screen(const SpriteInstance& instance, 
                                       int sprite_width, int sprite_height) const {
    // Calculate sprite bounds with scaling
    float scaled_width = sprite_width * std::abs(instance.scale_x);
    float scaled_height = sprite_height * std::abs(instance.scale_y);
    
    // Simple bounding box check (ignoring rotation for now)
    float left = instance.x - scaled_width * 0.5f;
    float right = instance.x + scaled_width * 0.5f;
    float top = instance.y - scaled_height * 0.5f;
    float bottom = instance.y + scaled_height * 0.5f;
    
    // Check if sprite is completely off screen
    if (right < 0 || left > screen_width_ || bottom < 0 || top > screen_height_) {
        return false;
    }
    
    return true;
}

void SpriteRenderer::render_sprite_instance(const SpriteInstance& instance) {
    // Get sprite texture from bank
    GLuint texture_id = sprite_bank_->get_texture(instance.sprite_slot);
    if (texture_id == 0) {
        return;
    }
    
    // Get sprite dimensions
    int sprite_width, sprite_height;
    if (!sprite_bank_->get_sprite_size(instance.sprite_slot, sprite_width, sprite_height)) {
        return;
    }
    
    // Skip if completely off screen
    if (!is_sprite_on_screen(instance, sprite_width, sprite_height)) {
        return;
    }
    
    // Bind the sprite texture
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    render_sprite_instance_optimized(instance);
}

void SpriteRenderer::render_sprite_instance_optimized(const SpriteInstance& instance) {
    // Get sprite dimensions (assume already validated)
    int sprite_width, sprite_height;
    if (!sprite_bank_->get_sprite_size(instance.sprite_slot, sprite_width, sprite_height)) {
        return;
    }
    
    // Set sprite color with alpha
    glColor4f(1.0f, 1.0f, 1.0f, instance.alpha);
    
    // Save current transform
    glPushMatrix();
    
    // Transform to sprite position and apply transformations
    glTranslatef(instance.x, instance.y, 0.0f);
    
    if (instance.rotation != 0.0f) {
        glRotatef(instance.rotation, 0.0f, 0.0f, 1.0f);
    }
    
    glScalef(instance.scale_x, instance.scale_y, 1.0f);
    
    // Calculate quad vertices (centered on origin)
    float half_width = sprite_width * 0.5f;
    float half_height = sprite_height * 0.5f;
    
    // Render textured quad
    glBegin(GL_QUADS);
        // Bottom-left
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(-half_width, half_height);
        
        // Bottom-right
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(half_width, half_height);
        
        // Top-right
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(half_width, -half_height);
        
        // Top-left
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(-half_width, -half_height);
    glEnd();
    
    // Restore transform
    glPopMatrix();
}

std::vector<int> SpriteRenderer::get_sorted_visible_sprites() const {
    std::lock_guard<std::mutex> lock(renderer_mutex_);
    
    std::vector<int> visible_sprites;
    
    // Collect all active sprites
    for (int i = 0; i < MAX_INSTANCES; ++i) {
        if (instances_[i].active) {
            visible_sprites.push_back(i);
        }
    }
    
    // Sort by z-order (lower z-order rendered first = behind)
    std::sort(visible_sprites.begin(), visible_sprites.end(), 
              [this](int a, int b) {
                  return instances_[a].z_order < instances_[b].z_order;
              });
    
    return visible_sprites;
}

} // namespace AbstractRuntime