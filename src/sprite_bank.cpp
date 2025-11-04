#include "sprite_bank.h"
#include <OpenGL/gl.h>
#include <iostream>
#include <algorithm>
#include <cairo/cairo.h>
#include <fstream>
#include <cstring>

namespace AbstractRuntime {

SpriteSlot::~SpriteSlot() {
    if (texture_id != 0) {
        glDeleteTextures(1, &texture_id);
    }
}

SpriteBank::SpriteBank()
    : initialized_(false)
    , sprite_count_(0) {
}

SpriteBank::~SpriteBank() {
    shutdown();
}

bool SpriteBank::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize all slots as empty
    for (int i = 0; i < BANK_SIZE; ++i) {
        slots_[i].occupied = false;
        slots_[i].texture_id = 0;
        slots_[i].width = DEFAULT_SPRITE_SIZE;
        slots_[i].height = DEFAULT_SPRITE_SIZE;
        slots_[i].source_file.clear();
    }
    
    sprite_count_ = 0;
    initialized_ = true;
    
    std::cout << "SpriteBank initialized with " << BANK_SIZE << " slots" << std::endl;
    return true;
}

void SpriteBank::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    // Release all textures
    for (int i = 0; i < BANK_SIZE; ++i) {
        if (slots_[i].occupied && slots_[i].texture_id != 0) {
            glDeleteTextures(1, &slots_[i].texture_id);
        }
        slots_[i] = SpriteSlot();  // Reset to default state
    }
    
    sprite_count_ = 0;
    initialized_ = false;
    
    std::cout << "SpriteBank shutdown complete" << std::endl;
}

bool SpriteBank::load_sprite(int slot, const std::string& filename) {
    if (!is_valid_slot(slot)) {
        std::cerr << "[Sprite Bank] ERROR: Invalid sprite slot " << slot << " (valid range: 0-" << (BANK_SIZE-1) << ")" << std::endl;
        return false;
    }
    

    
    // Queue the load request for processing on the main thread
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        load_queue_.push(SpriteLoadRequest(slot, filename));
    }
    

    return true;
}

void SpriteBank::release_sprite(int slot) {
    if (!is_valid_slot(slot) || !slots_[slot].occupied) {
        return;
    }
    
    // Note: This should be called with bank_mutex_ already locked
    
    if (slots_[slot].texture_id != 0) {
        glDeleteTextures(1, &slots_[slot].texture_id);
    }
    
    slots_[slot] = SpriteSlot();  // Reset to default state
    sprite_count_--;
}

GLuint SpriteBank::get_texture(int slot) const {
    if (!is_valid_slot(slot)) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    if (slots_[slot].occupied) {
        return slots_[slot].texture_id;
    }
    
    return 0;
}

bool SpriteBank::is_occupied(int slot) const {
    if (!is_valid_slot(slot)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    return slots_[slot].occupied;
}

bool SpriteBank::get_sprite_size(int slot, int& width, int& height) const {
    if (!is_valid_slot(slot)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    if (slots_[slot].occupied) {
        width = slots_[slot].width;
        height = slots_[slot].height;
        return true;
    }
    
    return false;
}

int SpriteBank::get_sprite_count() const {
    std::lock_guard<std::mutex> lock(bank_mutex_);
    return sprite_count_;
}

void SpriteBank::clear_all() {
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    for (int i = 0; i < BANK_SIZE; ++i) {
        if (slots_[i].occupied) {
            release_sprite(i);
        }
    }
    
    sprite_count_ = 0;
    std::cout << "All sprites cleared from bank" << std::endl;
}

int SpriteBank::process_load_queue() {
    int processed = 0;
    
    // Process all queued load requests
    while (true) {
        SpriteLoadRequest request(-1, "");
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (load_queue_.empty()) {
                break;
            }
            request = load_queue_.front();
            load_queue_.pop();
        }
        

        
        if (load_png_on_main_thread(request.filename, request.slot)) {
            processed++;

        } else {

        }
    }
    
    return processed;
}

bool SpriteBank::is_valid_slot(int slot) const {
    return slot >= 0 && slot < BANK_SIZE;
}

bool SpriteBank::load_png_texture(const std::string& filename, GLuint& texture_id, 
                                 int& width, int& height) {
    // This function is deprecated - use load_png_on_main_thread instead
    std::cerr << "[Sprite Bank] WARNING: load_png_texture called - use load_png_on_main_thread" << std::endl;
    return false;
}

bool SpriteBank::load_png_on_main_thread(const std::string& filename, int slot) {
    std::cout << "[Sprite Bank] Loading PNG on main thread: " << filename << " -> slot " << slot << std::endl;
    
    // Release existing sprite in slot
    {
        std::lock_guard<std::mutex> lock(bank_mutex_);
        if (slots_[slot].occupied) {
            release_sprite(slot);
        }
    }
    
    // Load PNG using Cairo
    cairo_surface_t* png_surface = cairo_image_surface_create_from_png(filename.c_str());
    
    if (cairo_surface_status(png_surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Sprite Bank] Failed to load PNG: " << filename 
                  << " - " << cairo_status_to_string(cairo_surface_status(png_surface)) << std::endl;
        if (png_surface) {
            cairo_surface_destroy(png_surface);
        }
        
        // Create fallback texture
        GLuint texture_id = create_empty_texture();
        if (texture_id != 0) {
            std::lock_guard<std::mutex> lock(bank_mutex_);
            slots_[slot].occupied = true;
            slots_[slot].texture_id = texture_id;
            slots_[slot].width = DEFAULT_SPRITE_SIZE;
            slots_[slot].height = DEFAULT_SPRITE_SIZE;
            slots_[slot].source_file = filename;
            sprite_count_++;
            return true;  // Fallback texture created successfully
        }
        return false;
    }
    
    int width = cairo_image_surface_get_width(png_surface);
    int height = cairo_image_surface_get_height(png_surface);
    
    if (width <= 0 || height <= 0) {
        std::cerr << "[Sprite Bank] Invalid PNG dimensions: " << width << "x" << height << std::endl;
        cairo_surface_destroy(png_surface);
        return false;
    }
    
    // Get pixel data from PNG surface directly
    unsigned char* cairo_data = cairo_image_surface_get_data(png_surface);
    int stride = cairo_image_surface_get_stride(png_surface);
    cairo_format_t format = cairo_image_surface_get_format(png_surface);
    
    if (format != CAIRO_FORMAT_ARGB32) {
        std::cerr << "[Sprite Bank] Unsupported PNG format for: " << filename << std::endl;
        cairo_surface_destroy(png_surface);
        return false;
    }
    
    // Convert Cairo ARGB32 to OpenGL RGBA with correct byte order
    std::vector<unsigned char> rgba_data(width * height * 4);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int cairo_idx = y * stride + x * 4;
            int rgba_idx = (y * width + x) * 4;
            
            // Cairo ARGB32 format on little-endian is stored as BGRA in memory
            unsigned char b = cairo_data[cairo_idx + 0];  // Blue at 0
            unsigned char g = cairo_data[cairo_idx + 1];  // Green at 1  
            unsigned char r = cairo_data[cairo_idx + 2];  // Red at 2
            unsigned char a = cairo_data[cairo_idx + 3];  // Alpha at 3
            
            // Convert to OpenGL RGBA format
            rgba_data[rgba_idx + 0] = r;    // Red
            rgba_data[rgba_idx + 1] = g;    // Green
            rgba_data[rgba_idx + 2] = b;    // Blue
            rgba_data[rgba_idx + 3] = a;    // Alpha
        }
    }
    
    // Create OpenGL texture (we're on main thread now)
    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data.data());
    
    // Set texture parameters for pixel-perfect sprites
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    cairo_surface_destroy(png_surface);
    
    // Store sprite information
    {
        std::lock_guard<std::mutex> lock(bank_mutex_);
        slots_[slot].occupied = true;
        slots_[slot].texture_id = texture_id;
        slots_[slot].width = width;
        slots_[slot].height = height;
        slots_[slot].source_file = filename;
        sprite_count_++;
    }

    std::cout << "[Sprite Bank] Successfully loaded PNG: " << filename 
              << " (" << width << "x" << height << ") -> texture " << texture_id << std::endl;
    
    return true;
}

GLuint SpriteBank::create_empty_texture() {

    GLuint texture_id = 0;
    glGenTextures(1, &texture_id);
    if (texture_id == 0) {
        std::cerr << "[Sprite Bank] ERROR: glGenTextures failed to create texture!" << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // Create a simple checkerboard pattern for debugging
    const int size = DEFAULT_SPRITE_SIZE;
    std::vector<uint32_t> pixels(size * size);
    
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool checker = ((x / 16) + (y / 16)) % 2;
            if (checker) {
                pixels[y * size + x] = 0xFF808080;  // Gray
            } else {
                pixels[y * size + x] = 0xFF404040;  // Dark gray
            }
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    

    return texture_id;
}

bool SpriteBank::save_sprite_as_png(int slot, const std::string& output_filename) {
    if (!is_valid_slot(slot)) {
        std::cerr << "[Sprite Bank] Cannot save invalid slot " << slot << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    if (!slots_[slot].occupied || slots_[slot].texture_id == 0) {
        std::cerr << "[Sprite Bank] Cannot save empty slot " << slot << std::endl;
        return false;
    }
    
    int width = slots_[slot].width;
    int height = slots_[slot].height;
    GLuint texture_id = slots_[slot].texture_id;
    
    // Read texture data from GPU
    std::vector<unsigned char> rgba_data(width * height * 4);
    
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data.data());
    
    // Create Cairo surface for PNG output
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Sprite Bank] Failed to create Cairo surface for saving" << std::endl;
        return false;
    }
    
    unsigned char* cairo_data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    
    // Convert RGBA to Cairo ARGB32 (premultiplied)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int rgba_idx = (y * width + x) * 4;
            int cairo_idx = y * stride + x * 4;
            
            unsigned char r = rgba_data[rgba_idx + 0];
            unsigned char g = rgba_data[rgba_idx + 1];
            unsigned char b = rgba_data[rgba_idx + 2];
            unsigned char a = rgba_data[rgba_idx + 3];
            
            // Convert to premultiplied alpha
            unsigned char r_pre, g_pre, b_pre;
            if (a == 255) {
                r_pre = r;
                g_pre = g;
                b_pre = b;
            } else if (a == 0) {
                r_pre = g_pre = b_pre = 0;
            } else {
                // Premultiply: premultiplied_color = color * alpha / 255
                r_pre = (unsigned char)((r * a + 127) / 255);
                g_pre = (unsigned char)((g * a + 127) / 255);
                b_pre = (unsigned char)((b * a + 127) / 255);
            }
            
            // Cairo ARGB32 format on little-endian is stored as BGRA bytes
            cairo_data[cairo_idx + 0] = b_pre;
            cairo_data[cairo_idx + 1] = g_pre;
            cairo_data[cairo_idx + 2] = r_pre;
            cairo_data[cairo_idx + 3] = a;
        }
    }
    
    cairo_surface_mark_dirty(surface);
    
    // Save as PNG
    cairo_status_t status = cairo_surface_write_to_png(surface, output_filename.c_str());
    cairo_surface_destroy(surface);
    
    if (status != CAIRO_STATUS_SUCCESS) {
        std::cerr << "[Sprite Bank] Failed to save PNG: " << output_filename 
                  << " - " << cairo_status_to_string(status) << std::endl;
        return false;
    }
    
    std::cout << "[Sprite Bank] Saved sprite slot " << slot << " to " << output_filename << std::endl;
    return true;
}

std::string SpriteBank::hash_sprite_data(int slot) {
    if (!is_valid_slot(slot)) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    if (!slots_[slot].occupied || slots_[slot].texture_id == 0) {
        return "";
    }
    
    int width = slots_[slot].width;
    int height = slots_[slot].height;
    GLuint texture_id = slots_[slot].texture_id;
    
    // Read texture data from GPU
    std::vector<unsigned char> rgba_data(width * height * 4);
    
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data.data());
    
    // Simple hash: sum of all bytes modulo large prime
    uint64_t hash = 0;
    const uint64_t prime = 1000000007ULL;
    
    for (size_t i = 0; i < rgba_data.size(); i++) {
        hash = (hash * 256 + rgba_data[i]) % prime;
    }
    
    char hash_str[32];
    snprintf(hash_str, sizeof(hash_str), "%016llx", (unsigned long long)hash);
    return std::string(hash_str);
}

std::string SpriteBank::hash_png_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    
    // Simple hash of file contents
    uint64_t hash = 0;
    const uint64_t prime = 1000000007ULL;
    
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        size_t bytes_read = file.gcount();
        for (size_t i = 0; i < bytes_read; i++) {
            hash = (hash * 256 + (unsigned char)buffer[i]) % prime;
        }
    }
    
    char hash_str[32];
    snprintf(hash_str, sizeof(hash_str), "%016llx", (unsigned long long)hash);
    return std::string(hash_str);
}

bool SpriteBank::get_texture_data(int slot, std::vector<unsigned char>& data, int& width, int& height) const {
    if (slot < 0 || slot >= BANK_SIZE) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(bank_mutex_);
    
    if (!slots_[slot].occupied || slots_[slot].texture_id == 0) {
        return false;
    }
    
    width = slots_[slot].width;
    height = slots_[slot].height;
    GLuint texture_id = slots_[slot].texture_id;
    
    // Read texture data from GPU
    data.resize(width * height * 4);
    
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    
    return true;
}

} // namespace AbstractRuntime