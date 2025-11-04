#include <cstring>

// Simple stub implementation for clipboard functions
// This provides a fallback when platform-specific clipboard is not available

bool get_clipboard_text(char* buffer, int buffer_size) {
    // Stub implementation - return empty string
    if (buffer && buffer_size > 0) {
        buffer[0] = '\0';
    }
    return false;
}

bool set_clipboard_text(const char* text) {
    // Stub implementation - do nothing
    (void)text; // Suppress unused parameter warning
    return false;
}