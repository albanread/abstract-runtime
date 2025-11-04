#include <string>
#include <cstring>

#ifdef __APPLE__
#include <AppKit/AppKit.h>

extern "C" {

bool get_clipboard_text(char* buffer, int buffer_size) {
    if (!buffer || buffer_size <= 0) {
        return false;
    }
    
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        NSString* string = [pasteboard stringForType:NSPasteboardTypeString];
        
        if (string == nil) {
            buffer[0] = '\0';
            return false;
        }
        
        const char* cString = [string UTF8String];
        if (cString == nullptr) {
            buffer[0] = '\0';
            return false;
        }
        
        size_t len = strlen(cString);
        if (len >= static_cast<size_t>(buffer_size)) {
            // Truncate to fit buffer
            strncpy(buffer, cString, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        } else {
            strcpy(buffer, cString);
        }
        
        return true;
    }
}

bool set_clipboard_text(const char* text) {
    if (!text) {
        return false;
    }
    
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        NSString* string = [NSString stringWithUTF8String:text];
        if (string == nil) {
            return false;
        }
        
        BOOL success = [pasteboard setString:string forType:NSPasteboardTypeString];
        return success == YES;
    }
}

} // extern "C"

#else

// Fallback implementation for non-macOS platforms
extern "C" {

bool get_clipboard_text(char* buffer, int buffer_size) {
    if (buffer && buffer_size > 0) {
        buffer[0] = '\0';
    }
    return false;
}

bool set_clipboard_text(const char* text) {
    (void)text; // Suppress unused parameter warning
    return false;
}

} // extern "C"

#endif // __APPLE__