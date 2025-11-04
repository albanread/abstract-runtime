# Sprite Cascade Demo

A modern C++ application demonstrating the Abstract Runtime's sprite system capabilities.

## Overview

This demo showcases a physics-based particle system using all 128 available sprite slots. Sprites cascade from the center of the screen with realistic physics including gravity, bouncing, rotation, and fading effects.

## Features

### Modern C++ Design
- **RAII**: Automatic resource management with smart pointers
- **Object-Oriented**: Clean separation of concerns with dedicated classes
- **Type Safety**: Modern C++ constructs and strong typing
- **No Console I/O**: Pure graphics application compatible with runtime requirements

### Physics Simulation
- **Gravity**: Realistic downward acceleration
- **Air Resistance**: Gradual velocity dampening
- **Collision Detection**: Ground and wall bouncing with energy loss
- **Rotation**: Dynamic rotation speed affected by collisions
- **Fading**: Particles fade out over time before despawning

### Interactive Controls
- **SPACE**: Pause/Resume the simulation
- **+/=**: Increase spawn rate (up to 10 particles/second)
- **-**: Decrease spawn rate (minimum 1 particle/second)
- **R**: Reset the demo with new random parameters
- **ESC**: Exit the application

### Visual Features
- **Loading Progress**: Visual progress bar during sprite loading
- **Activity Indicator**: Top-left dots showing active particle count
- **Spawn Progress**: Bottom progress bar showing spawn completion
- **Pause Indicator**: Visual pause symbol when simulation is paused

## Technical Details

### Performance
- **Target**: 60 FPS with frame limiting
- **Particle Count**: Up to 256 particles (limited by available sprites)
- **Efficient Updates**: Only active particles are processed
- **Memory Management**: Smart pointers prevent memory leaks

### Sprite Management
- **Asset Loading**: Automatically loads sprite001.png through sprite128.png
- **Instance Management**: Each particle gets its own sprite instance
- **Visual Properties**: Scale, rotation, alpha, and position per particle
- **Cleanup**: Automatic sprite hiding when particles deactivate

### Physics Parameters
```cpp
constexpr float GRAVITY = 150.0f;           // Downward acceleration
constexpr float AIR_RESISTANCE = 0.998f;    // Velocity dampening
constexpr float BOUNCE_DAMPING = 0.7f;      // Energy loss on bounce
constexpr int MAX_BOUNCES = 4;              // Maximum bounces before sticking
constexpr float FADE_START_TIME = 12.0f;    // When fading begins (seconds)
constexpr float FADE_RATE = 0.5f;           // How quickly particles fade
```

## Building

### Prerequisites
- C++17 compatible compiler (clang++ recommended)
- Abstract Runtime library built
- All sprite assets (sprite001.png - sprite128.png) in assets/ directory

### Compilation
```bash
clang++ -std=c++17 -Wall -Wextra -O2 -g \
    -mmacosx-version-min=10.15 \
    -Iinclude -I../include \
    -I/opt/homebrew/include -I/usr/local/include \
    -I/opt/homebrew/include/freetype2 \
    -I/opt/homebrew/include/cairo \
    -I/opt/homebrew/include/lua5.4 \
    sprite_cascade_demo.cpp \
    -Lbuild -labstract_runtime \
    -L/opt/homebrew/lib -L/usr/local/lib \
    -lSDL2 -framework OpenGL -lpng -ljpeg \
    -lfreetype -lcairo -pthread -lgtest -lgtest_main -llua \
    -o sprite_cascade_demo
```

### Using Makefile
```bash
# Add to Makefile targets:
sprite_cascade_demo: sprite_cascade_demo.cpp $(LIBRARY) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -labstract_runtime $(LIBS) -o $@

# Then build:
make sprite_cascade_demo
```

## Running

```bash
./sprite_cascade_demo
```

The application will:
1. Initialize the Abstract Runtime in 800x600 mode
2. Load all available sprite assets with visual progress
3. Create a particle pool and begin the cascade effect
4. Run until ESC is pressed or window is closed
5. Automatically reset when all particles have faded

## Architecture

### Core Classes

#### `RandomGenerator`
- Modern C++ random number generation using `<random>`
- Thread-safe random float and integer generation
- Specialized methods for angles and ranges

#### `Vector2`
- Lightweight 2D vector class for physics calculations
- Overloaded operators for mathematical operations
- Factory method for angle-based vector creation

#### `Particle`
- Represents a single sprite with full physics state
- Manages its own sprite instance lifecycle
- Handles collision detection and response
- Automatic deactivation when off-screen or faded

#### `SpriteCascadeDemo`
- Main application controller
- Manages particle pool and spawning logic
- Handles input processing and state management
- Coordinates rendering and updates

### Design Patterns Used

- **RAII**: Automatic resource management
- **Object Pool**: Reusable particle instances
- **State Machine**: Demo states (loading, running, paused)
- **Strategy Pattern**: Configurable physics parameters

## Comparison to Original

### Improvements Over test_real_sprites.cpp

1. **No Console Output**: Pure graphics application
2. **Modern C++**: Smart pointers, RAII, proper OOP design
3. **Better Physics**: More realistic collision and fading
4. **Interactive Controls**: Real-time parameter adjustment
5. **Visual Feedback**: Loading progress and status indicators
6. **Cleaner Code**: Better separation of concerns
7. **Type Safety**: Proper const-correctness and type conversions

### Maintained Features
- All 128 sprites loaded and utilized
- Realistic physics simulation
- Performance optimization for 60 FPS
- Automatic demo reset cycle

## Assets Required

The demo expects sprite assets in the `assets/` directory:
- `sprite001.png` through `sprite128.png`
- PNG format, any reasonable size (sprites are auto-scaled)
- Transparent background recommended for best visual effect

## Troubleshooting

### Common Issues

**No sprites appear**: Check that sprite assets are in `assets/` directory with correct naming

**Poor performance**: Reduce spawn rate with '-' key, or check system capabilities

**Input not working**: Ensure window has focus and try different key combinations

**Build errors**: Verify all dependencies are installed and library is built

### Debug Features

The demo includes several visual indicators to help diagnose issues:
- Loading progress bar shows asset loading status
- Activity dots show current active particle count
- Spawn progress bar shows how many particles have been created
- Pause indicators show simulation state

## License

This demo is part of the Abstract Runtime project and follows the same licensing terms.