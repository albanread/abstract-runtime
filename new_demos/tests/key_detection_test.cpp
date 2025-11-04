#include "../../include/abstract_runtime.h"
#include "../../include/input_system.h"
#include <chrono>
#include <thread>

using namespace AbstractRuntime;

/**
 * Key Detection Test Program
 * 
 * This program demonstrates and documents the different key detection functions
 * in the Abstract Runtime and their behaviors with different key types.
 */

void run_key_detection_test() {
    console_section("Key Detection Test Program");
    console_info("This program tests and documents key detection behavior");
    console_info("Press different keys to see how they're detected by different functions");
    console_separator();
    
    // Set up display
    set_background_color(20, 20, 40);
    clear_text();
    clear_graphics();
    
    // Initialize input system
    if (!init_input_system()) {
        console_error("Failed to initialize input system!");
        return;
    }
    console_info("Input system initialized successfully");
    
    // Display instructions
    set_text_ink(255, 255, 0, 255); // Yellow
    print_at(2, 1, "Key Detection Test Program");
    
    set_text_ink(255, 255, 255, 255); // White
    print_at(2, 3, "Test Instructions:");
    print_at(4, 4, "1. Try typing letters, numbers, symbols");
    print_at(4, 5, "2. Try arrow keys (Left, Right, Up, Down)");
    print_at(4, 6, "3. Try function keys (F1-F12)");
    print_at(4, 7, "4. Try special keys (Space, Tab, Enter)");
    print_at(4, 8, "5. Press ESC to exit");
    
    set_text_ink(0, 255, 255, 255); // Cyan
    print_at(2, 10, "Key Detection Results:");
    
    console_info("Starting key detection loop...");
    console_info("Press keys to see detection results in both console and display");
    
    int line = 12;
    int frame_count = 0;
    
    while (!should_quit()) {
        // Clear old results if we've filled the screen
        if (line > 24) {
            // Clear results area
            for (int i = 12; i <= 24; i++) {
                for (int j = 0; j < 80; j++) {
                    print_at(j, i, " ");
                }
            }
            line = 12;
        }
        
        // Test key() function
        int key_result = key();
        static int last_key = -1;
        
        // Test any_key_pressed() function  
        int any_key_result = any_key_pressed();
        static int last_any_key = -1;
        
        // Test specific is_key_pressed() functions
        bool arrow_keys[4] = {
            is_key_pressed(INPUT_KEY_LEFT),
            is_key_pressed(INPUT_KEY_RIGHT), 
            is_key_pressed(INPUT_KEY_UP),
            is_key_pressed(INPUT_KEY_DOWN)
        };
        
        bool function_keys[4] = {
            is_key_pressed(INPUT_KEY_F1),
            is_key_pressed(INPUT_KEY_F2),
            is_key_pressed(INPUT_KEY_F3),
            is_key_pressed(INPUT_KEY_F4)
        };
        
        // Report key() detection
        if (key_result != last_key && key_result != -1) {
            char display_line[100];
            snprintf(display_line, sizeof(display_line), 
                    "key() = %d (ASCII: '%c')", 
                    key_result, 
                    (key_result >= 32 && key_result <= 126) ? (char)key_result : '?');
            
            set_text_ink(0, 255, 0, 255); // Green
            print_at(4, line++, display_line);
            
            console_printf("key() detected: %d (ASCII: '%c')", 
                          key_result, 
                          (key_result >= 32 && key_result <= 126) ? (char)key_result : '?');
            last_key = key_result;
        }
        
        // Report any_key_pressed() detection
        if (any_key_result != last_any_key && any_key_result != -1) {
            char display_line[100];
            const char* key_name = "Unknown";
            
            // Identify the key
            if (any_key_result >= 32 && any_key_result <= 126) {
                key_name = "ASCII char";
            } else {
                switch (any_key_result) {
                    case INPUT_KEY_LEFT: key_name = "LEFT ARROW"; break;
                    case INPUT_KEY_RIGHT: key_name = "RIGHT ARROW"; break;
                    case INPUT_KEY_UP: key_name = "UP ARROW"; break;
                    case INPUT_KEY_DOWN: key_name = "DOWN ARROW"; break;
                    case INPUT_KEY_F1: key_name = "F1"; break;
                    case INPUT_KEY_F2: key_name = "F2"; break;
                    case INPUT_KEY_F3: key_name = "F3"; break;
                    case INPUT_KEY_F4: key_name = "F4"; break;
                    case INPUT_KEY_F5: key_name = "F5"; break;
                    case INPUT_KEY_F6: key_name = "F6"; break;
                    case INPUT_KEY_F7: key_name = "F7"; break;
                    case INPUT_KEY_F8: key_name = "F8"; break;
                    case INPUT_KEY_F9: key_name = "F9"; break;
                    case INPUT_KEY_F10: key_name = "F10"; break;
                    case INPUT_KEY_F11: key_name = "F11"; break;
                    case INPUT_KEY_F12: key_name = "F12"; break;
                    case 27: key_name = "ESCAPE"; break;
                    case 9: key_name = "TAB"; break;
                    case 13: key_name = "ENTER"; break;
                }
            }
            
            snprintf(display_line, sizeof(display_line), 
                    "any_key_pressed() = %d (%s)", 
                    any_key_result, key_name);
            
            set_text_ink(255, 200, 0, 255); // Orange  
            print_at(4, line++, display_line);
            
            console_printf("any_key_pressed() detected: %d (%s)", any_key_result, key_name);
            last_any_key = any_key_result;
        }
        
        // Show real-time key state at bottom
        set_text_ink(150, 150, 150, 255); // Gray
        print_at(2, 26, "Current State:");
        
        char state_line[100];
        snprintf(state_line, sizeof(state_line), 
                "key()=%d  any_key()=%d  Arrows: L=%d R=%d U=%d D=%d", 
                key_result, any_key_result,
                arrow_keys[0] ? 1 : 0, arrow_keys[1] ? 1 : 0, 
                arrow_keys[2] ? 1 : 0, arrow_keys[3] ? 1 : 0);
        print_at(4, 27, state_line);
        
        // Check for exit condition
        if (key_result == 27) { // ESC
            console_info("Escape pressed - exiting test");
            break;
        }
        
        // Periodic status
        if (++frame_count % 300 == 0) {
            console_printf("Test running... %d frames processed", frame_count);
        }
        
        // Frame rate control
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    // Summary
    console_section("Key Detection Test Complete");
    console_info("Key detection behavior summary:");
    console_info("  key() - Detects ASCII keys (32-126) + basic special keys");
    console_info("  is_key_pressed() - Detects extended keys (arrows, function keys, etc.)");
    console_info("  any_key_pressed() - Combines both methods for complete coverage");
    console_separator();
    
    // Display completion message
    clear_text();
    set_text_ink(0, 255, 0, 255);
    print_at(25, 12, "Test Complete!");
    print_at(20, 14, "Check console for detailed results");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

int main() {
    console_section("Abstract Runtime - Key Detection Test");
    console_info("Testing key detection functions and documenting behavior");
    
    // Use run_runtime_with_app for proper initialization
    return run_runtime_with_app(SCREEN_800x600, run_key_detection_test);
}