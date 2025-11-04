# Abstract Runtime - Test Programs

This directory contains test programs for validating and documenting Abstract Runtime behavior. These tests serve both as debugging tools and as documentation for developers.

## Available Tests

### Lua Test Runner (`lua_test_runner.cpp`)

**Purpose**: Modern, console-based test runner for executing Lua test scripts that validate Abstract Runtime Lua bindings and functionality.

**Key Features**:
- **Automatic test discovery** - Finds all `test_*.lua` or `*_test.lua` files
- **Console-based output** - Uses runtime's console methods for clear reporting  
- **Dual execution modes** - Headless (console-only) or graphics mode
- **Comprehensive reporting** - Detailed test results with timing and error info
- **Test assertions** - Built-in assertion functions for Lua tests
- **Verbose mode** - Optional detailed output for debugging

**How to run**:
```bash
# Auto-discover and run all tests (graphics mode)
make lua_tests

# Run specific test file
./build/bin/lua_test_runner ../lua_tests/test_modernized_basic.lua

# Run in headless mode (no graphics)
./build/bin/lua_test_runner --headless

# Run with verbose output
./build/bin/lua_test_runner -v

# Show help
./build/bin/lua_test_runner --help
```

**Test File Requirements**:
- Must be `.lua` files starting with `test_` or ending with `_test.lua`
- Placed in `lua_tests/` directory (or specified path)
- Can use assertion functions: `assert_equals()`, `assert_true()`, `assert_false()`, `assert_not_nil()`, `assert_nil()`
- Can use `print()` for test output (captured by test runner)

**Example Test File** (`test_example.lua`):
```lua
-- Example Lua Test
print("=== Example Test ===")

-- Test runtime functions
assert_true(get_screen_width() > 0, "Screen width should be positive")
assert_equals("hello", "hello", "String equality test")

-- Test graphics
set_background_color(100, 150, 200)
clear_graphics()
set_draw_color(255, 0, 0, 255)
draw_rect(100, 100, 50, 30)

print("Test completed successfully!")
```

**Console Output Example**:
```
=== Modern Lua Test Runner for Abstract Runtime ===
[INFO] Advanced test runner with console logging and auto-discovery

=== Test Discovery ===
Searching for tests in: lua_tests/
  Found test: test_modernized_basic.lua
  Found test: test_graphics_system.lua
Discovered 2 test files

=== Running Lua Tests ===
Running test: test_modernized_basic.lua
  PASSED (0.000s)
Running test: test_graphics_system.lua  
  PASSED (0.234s)

=== Test Results Summary ===
Tests run: 2, Passed: 2, Failed: 0
🎉 ALL TESTS PASSED!
```

### Key Detection Test (`key_detection_test.cpp`)

**Purpose**: Documents and validates the different key detection functions in the Abstract Runtime.

**What it tests**:
- `key()` function behavior with ASCII and extended keys
- `is_key_pressed()` function behavior with different key types
- `any_key_pressed()` helper function (combines both approaches)
- Real-time key detection and reporting

**Key findings documented**:
- **`key()` function**: Only detects ASCII characters (32-126) and basic special keys (Space, Tab, Enter, Escape, Backspace, Delete)
- **`is_key_pressed()` function**: Detects extended keys (Arrow keys 200-203, Function keys 210-221, Modifier keys 230+)
- **`any_key_pressed()` function**: Combines both methods for complete key coverage

**How to run**:
```bash
# From abstract_runtime directory
make run_key_detection_test

# Or from new_demos directory
make run-key-detection-test

# Or quick run
make key_test
```

**What you'll see**:
- Real-time console output showing detected keys and their codes
- On-screen display showing detection results from different functions
- Clear documentation of which function detects which key types

**Expected behavior**:
```
# Typing 'hello' shows:
key() detected: 104 (ASCII: 'h')
any_key_pressed() detected: 104 (ASCII char)
key() detected: 101 (ASCII: 'e')
any_key_pressed() detected: 101 (ASCII char)
...

# Pressing arrow keys shows:
any_key_pressed() detected: 200 (LEFT ARROW)
any_key_pressed() detected: 201 (RIGHT ARROW)
# Note: key() does NOT detect arrow keys

# Pressing function keys shows:
any_key_pressed() detected: 210 (F1)
any_key_pressed() detected: 211 (F2)
```

## Console Output Integration

All tests use the Abstract Runtime's console output functions for debugging:

- `console_info()` - General information
- `console_warning()` - Warnings about missing assets or issues
- `console_error()` - Error conditions
- `console_printf()` - Formatted output
- `console_section()` - Section headers
- `console_separator()` - Visual separators

This provides detailed runtime information without breaking the graphics context.

## Test Development Guidelines

When creating new tests:

1. **Use console functions liberally** - They're invaluable for debugging
2. **Document expected behavior** - Include comments about what should happen
3. **Test edge cases** - Include boundary conditions and error states
4. **Provide clear instructions** - Make it easy for others to run and understand
5. **Follow the pattern** - Use `run_runtime_with_app()` for proper initialization

## Example Test Structure

```cpp
#include "../include/abstract_runtime.h"
#include "../include/input_system.h"

void run_my_test() {
    console_section("My Test Program");
    console_info("Testing specific runtime behavior");
    
    // Setup
    set_background_color(20, 20, 40);
    clear_text();
    
    if (!init_input_system()) {
        console_error("Failed to initialize input system!");
        return;
    }
    
    // Test loop
    while (!should_quit()) {
        // Test logic here
        // Use console_printf() to report findings
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    console_section("Test Complete");
    console_info("Summary of findings");
}

int main() {
    return run_runtime_with_app(SCREEN_800x600, run_my_test);
}
```

## Adding New Tests

1. **Create the test file** in `new_demos/tests/`
2. **Add build target** to `new_demos/Makefile`:
   ```makefile
   $(BIN_DIR)/my_test: tests/my_test.cpp $(RUNTIME_LIB) | $(BIN_DIR)
   	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@
   ```
3. **Add run target**:
   ```makefile
   run-my-test: $(BIN_DIR)/my_test
   	cd .. && ./new_demos/$(BIN_DIR)/my_test
   ```
4. **Update help text** to include the new test
5. **Document the test** in this README

## Debugging Tips

- **Use console output extensively** - It's visible in the terminal while the app runs
- **Test incrementally** - Build up complex tests from simple cases
- **Check console for detailed status** - Apps provide much more info than just the graphics
- **Press F8/F9** to toggle the LuaJIT REPL overlay for interactive testing
- **Use ESC to exit** most tests gracefully

## Future Test Ideas

- **Sprite allocation stress test** - Test the thread-safe sprite allocator under load
- **Multi-threading performance test** - Measure LuaJIT threading overhead
- **Graphics API coverage test** - Validate all drawing functions work correctly
- **Memory usage test** - Monitor resource usage over time
- **Input lag measurement** - Quantify input response times
- **Asset loading test** - Test various asset types and error handling
- **Text rendering test** - Validate font rendering in all column modes

These tests serve as both validation tools and living documentation of runtime behavior. They're especially valuable when making changes to the runtime or when debugging issues reported by developers.

**Happy testing! 🧪**