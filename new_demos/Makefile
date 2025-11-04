# NewBCPL Abstract Runtime - Modern C++ Demos Makefile
# Builds modern C++ applications using the abstract runtime

CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -I../include -I../../include
LIBS = -lSDL2 -lGL -lpng -ljpeg -lfreetype -pthread -lluajit-5.1

# Platform-specific settings
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS specific settings
    CXXFLAGS += -mmacosx-version-min=10.15
    INCLUDES += -I/opt/homebrew/include -I/usr/local/include -I/opt/homebrew/include/freetype2 -I/opt/homebrew/include/cairo -I/opt/homebrew/opt/luajit/include/luajit-2.1
    LIBS = -L/opt/homebrew/lib -L/usr/local/lib -L/opt/homebrew/opt/luajit/lib -lSDL2 -framework OpenGL -lpng -ljpeg -lfreetype -lcairo -pthread -lgtest -lgtest_main -lluajit-5.1
endif

# Build directories
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
RUNTIME_LIB = ../build/libabstract_runtime.a

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR): | $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

# Check if runtime library exists
$(RUNTIME_LIB):
	@echo "Error: Abstract Runtime library not found at $(RUNTIME_LIB)"
	@echo "Please build the runtime first by running 'make library' in the parent directory"
	@exit 1

# Demo targets
all: sprite_cascade_demo graphics_showcase_demo unlimited_scroll_demo tests

# Test targets
tests: $(BIN_DIR)/key_detection_test $(BIN_DIR)/lua_test_runner

sprite_cascade_demo: $(BIN_DIR)/sprite_cascade_demo

graphics_showcase_demo: $(BIN_DIR)/graphics_showcase_demo

unlimited_scroll_demo: $(BIN_DIR)/unlimited_scroll_demo

$(BIN_DIR)/sprite_cascade_demo: sprite_cascade_demo.cpp $(RUNTIME_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@

$(BIN_DIR)/graphics_showcase_demo: graphics_showcase_demo.cpp $(RUNTIME_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@

$(BIN_DIR)/unlimited_scroll_demo: unlimited_scroll_demo.cpp $(RUNTIME_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@

$(BIN_DIR)/key_detection_test: tests/key_detection_test.cpp $(RUNTIME_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@

$(BIN_DIR)/lua_test_runner: tests/lua_test_runner.cpp $(RUNTIME_LIB) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L../build -labstract_runtime $(LIBS) -o $@

# Run targets
run-sprite-cascade: $(BIN_DIR)/sprite_cascade_demo
	cd .. && ./new_demos/$(BIN_DIR)/sprite_cascade_demo

run-graphics-showcase: $(BIN_DIR)/graphics_showcase_demo
	cd .. && ./new_demos/$(BIN_DIR)/graphics_showcase_demo

run-unlimited-scroll: $(BIN_DIR)/unlimited_scroll_demo
	cd .. && ./new_demos/$(BIN_DIR)/unlimited_scroll_demo

run-key-detection-test: $(BIN_DIR)/key_detection_test
	cd .. && ./new_demos/$(BIN_DIR)/key_detection_test

run-lua-test-runner: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner

# Quick build and run
demo: $(BIN_DIR)/sprite_cascade_demo
	cd .. && ./new_demos/$(BIN_DIR)/sprite_cascade_demo

graphics: $(BIN_DIR)/graphics_showcase_demo
	cd .. && ./new_demos/$(BIN_DIR)/graphics_showcase_demo

scroll: $(BIN_DIR)/unlimited_scroll_demo
	cd .. && ./new_demos/$(BIN_DIR)/unlimited_scroll_demo

key_test: $(BIN_DIR)/key_detection_test
	cd .. && ./new_demos/$(BIN_DIR)/key_detection_test

lua_tests: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner

# Modern test runner targets
test-smoke: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner --filter "basic|console" --verbose

test-performance: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner --category performance --verbose

test-headless: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner --headless --exclude-category graphics

test-graphics: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner --category graphics --verbose

test-modern: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner test_modern_features.lua --verbose

test-quick: $(BIN_DIR)/lua_test_runner
	cd .. && ./new_demos/$(BIN_DIR)/lua_test_runner --filter "modernized_basic|console_integration" --verbose

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Help
help:
	@echo "Modern C++ Demos Makefile"
	@echo "=========================="
	@echo ""
	@echo "Targets:"
	@echo "  all                   - Build all demos and tests"
	@echo "  sprite_cascade_demo   - Build sprite cascade demo"
	@echo "  graphics_showcase_demo - Build graphics showcase demo"
	@echo "  unlimited_scroll_demo - Build unlimited scroll demo"
	@echo "  tests                 - Build all test programs"
	@echo "  run-sprite-cascade    - Build and run sprite cascade demo"
	@echo "  run-graphics-showcase - Build and run graphics showcase demo"
	@echo "  run-unlimited-scroll  - Build and run unlimited scroll demo"
	@echo "  run-key-detection-test - Build and run key detection test"
	@echo "  run-lua-test-runner    - Build and run lua test runner"
	@echo "  demo                  - Quick build and run sprite cascade"
	@echo "  graphics              - Quick build and run graphics showcase"
	@echo "  scroll                - Quick build and run unlimited scroll"
	@echo "  key_test              - Quick build and run key detection test"
	@echo "  lua_tests             - Quick build and run lua test runner"
	@echo "  clean                 - Remove build files"
	@echo "  help                  - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - Abstract Runtime library must be built first"
	@echo "  - All sprite assets (sprite001.png - sprite128.png) in ../assets/"
	@echo ""

.PHONY: all clean help run-sprite-cascade run-graphics-showcase run-unlimited-scroll run-key-detection-test run-lua-test-runner demo graphics scroll key_test lua_tests tests sprite_cascade_demo graphics_showcase_demo unlimited_scroll_demo
