# Abstract Runtime - Modern C++ Demos

This directory contains modernized C++ demo applications showcasing the Abstract Runtime's graphics, sprite, and LuaJIT capabilities.

## Prerequisites

1. **Build the Abstract Runtime library first:**
   ```bash
   cd ..
   make library
   ```

2. **Ensure sprite assets are available:**
   - The demos expect sprite files in `../assets/` 
   - Required: `sprite001.png` through `sprite128.png` (128×128 pixels each)
   - If assets are missing, sprites will appear as checkerboard patterns

## Available Demos

### Sprite Cascade Demo
Demonstrates cascading sprite animations with physics and movement patterns.

**Build and run:**
```bash
make run-sprite-cascade
```

**Or manually:**
```bash
make sprite_cascade_demo
cd .. && ./new_demos/build/bin/sprite_cascade_demo
```

### Graphics Showcase Demo  
Comprehensive demonstration of the runtime's graphics capabilities including vector drawing, text rendering, and sprite management.

**Build and run:**
```bash
make run-graphics-showcase  
```

**Or manually:**
```bash
make graphics_showcase_demo
cd .. && ./new_demos/build/bin/graphics_showcase_demo
```

## Quick Commands

```bash
# Build all demos
make all

# Build and run sprite cascade (quick)
make demo

# Build and run graphics showcase (quick)  
make graphics

# Build and run unlimited scroll (quick)
make scroll

# Build and run key detection test (quick)
make key_test

# Clean build files
make clean

# Build all tests
make tests

# Show help
make help
```

## LuaJIT Integration Example

The demos also showcase LuaJIT multi-threading. To test the Lua integration:

1. **Start the LuaJIT REPL:**
   ```bash
   cd ..
   make lua_repl
   ./build/bin/lua_repl
   ```

2. **In the REPL, try executing Lua scripts:**
   ```lua
   -- Execute a bouncing ball animation in a separate thread
   thread_id = exec_lua('lua_examples/bouncing_ball.lua')
   
   -- Execute multiple scripts simultaneously
   exec_lua('lua_examples/text_ticker.lua')
   exec_lua('lua_examples/particle_system.lua')
   
   -- Check active threads
   print("Active threads: " .. get_thread_count())
   
   -- Stop a thread
   stop_lua_thread(thread_id)
   ```

## Testing & Debugging

The `tests/` subdirectory contains test programs for validating runtime behavior:

### Key Detection Test
Documents and tests the different input functions:
```bash
make run-key-detection-test
# or
make key_test
```

This test reveals important behavior:
- `key()` - Only detects ASCII keys (letters, numbers, space, tab, escape)
- `is_key_pressed()` - Detects extended keys (arrows, function keys, modifiers)  
- `any_key_pressed()` - New helper function that combines both approaches

**Key insight**: Arrow keys need `is_key_pressed()`, not `key()`!

## Input Handling Important Notes

**Critical Discovery**: The Abstract Runtime has different input functions for different key types:

- **`key()` function**: Works for ASCII keys (Space=32, Tab=9, Escape=27, letters, numbers) 
- **`is_key_pressed()` function**: Works for extended keys (Arrow keys=200-203, function keys, etc.)
- **`key_just_pressed()`**: Use for one-time key press detection (edge detection)

**Example Usage**:
```cpp
// Control keys (ASCII) - use key() for detection
int current_key = key();
if (current_key == 32) { // Space pressed
    // Handle space key
}

// Arrow keys (extended) - use is_key_pressed()
if (is_key_pressed(INPUT_KEY_LEFT)) {
    // Handle left arrow (continuously while held)
}
if (key_just_pressed(INPUT_KEY_SPACE)) {
    // Handle space press (once per press)
}
```

## Console Output Functions

All demos now support console debugging via the new console functions:
```cpp
console_info("Initialization complete");
console_warning("Asset missing but continuing");
console_error("Critical error occurred");
console_printf("Frame count: %d", frame_num);
console_section("New Phase Starting");
```

These functions output to the terminal while the app runs in graphics mode, making debugging much easier!

## Asset Organization

The runtime expects this directory structure:
```
abstract_runtime/
├── assets/
│   ├── sprite001.png
│   ├── sprite002.png  
│   ├── ...
│   ├── sprite128.png
│   └── tiles/
│       ├── tile001_sky_light.png
│       ├── tile008_grass_green.png
│       └── ...
├── new_demos/
│   ├── sprite_cascade_demo.cpp
│   ├── graphics_showcase_demo.cpp
│   ├── unlimited_scroll_demo.cpp
│   ├── tests/
│   │   ├── key_detection_test.cpp
│   │   └── README.md
│   └── README.md (this file)
└── lua_examples/
    ├── simple_demo.lua
    ├── bouncing_ball.lua
    ├── text_ticker.lua
    └── particle_system.lua
```

## Troubleshooting

**"Failed to initialize runtime"**
- Ensure you have SDL2, OpenGL, and other dependencies installed
- On macOS: `brew install sdl2 freetype cairo luajit`

**Checkerboard sprites**
- Sprite PNG files are missing from `../assets/`
- Verify file permissions and paths

**Build errors**
- Make sure the Abstract Runtime library was built first: `cd .. && make library`
- Check that all dependencies are installed

**LuaJIT not found**
- Install LuaJIT: `brew install luajit` (macOS) or `apt-get install libluajit-5.1-dev` (Linux)
- Update library paths in Makefile if needed

## Performance Notes

- Demos run at 60 FPS with automatic vsync
- Each Lua script runs in its own thread with isolated state
- Runtime API calls are synchronized with a global mutex for thread safety
- For best performance, avoid excessive string operations in tight loops

## Next Steps

For more advanced usage, see:
- `../LUAJIT_MULTITHREADING.md` - Comprehensive LuaJIT threading documentation
- `../README.md` - Full Abstract Runtime API reference
- `../lua_examples/` - More Lua script examples

## Development Tips

1. **Use console functions liberally** - They make debugging infinitely easier
2. **Test input handling carefully** - Different key types need different functions
3. **Check console output** - Apps provide detailed status information during execution
4. **Missing assets are OK** - Demos gracefully degrade with colored rectangles/squares
5. **Frame rate matters** - Use `std::this_thread::sleep_for()` to control update speed

## Available Demos

### 1. Sprite Cascade Demo
Classic sprite animation with physics and cascading effects.

### 2. Graphics Showcase Demo  
Comprehensive demonstration of vector graphics, text, and sprite capabilities.

### 3. Unlimited Scroll Demo
Large tile-based world with smooth unlimited scrolling and viewport shifting.
- **60x60 tile world** (3600 tiles total)
- **Viewport shifting optimization** for performance
- **Manual and automatic scrolling modes**
- **Real-time debug information**
- **Console logging** shows initialization and runtime status

## Test Programs

### Key Detection Test (`tests/`)
Essential test that documents input function behavior and helps debug input issues.
- **Documents key detection differences** between `key()` and `is_key_pressed()`
- **Tests all key types** - ASCII, arrows, function keys, modifiers
- **Real-time console feedback** shows exactly what's detected
- **Invaluable for input debugging** - saved us hours when arrow keys weren't working!

Happy coding! 🚀