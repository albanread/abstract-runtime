-- Basic Runtime Functionality Test
-- Tests core runtime initialization and display functions

print("=== Basic Runtime Test ===")

-- Test 1: Runtime should not be initialized yet
print("Test 1: Check initial runtime state")
local should_be_false = should_quit()
assert_false(should_be_false, "Runtime should not be quitting initially")

-- Test 2: Initialize the runtime
print("Test 2: Initialize runtime")
local init_result = init_abstract_runtime(1)  -- 800x600 mode
assert_true(init_result, "Runtime initialization should succeed")

-- Test 3: Check screen dimensions
print("Test 3: Check screen dimensions")
local width = get_screen_width()
local height = get_screen_height()
assert_true(width > 0, "Screen width should be positive")
assert_true(height > 0, "Screen height should be positive")
print("Screen size: " .. width .. "x" .. height)

-- Test 4: Test background color setting
print("Test 4: Test background color")
set_background_color(64, 128, 192)  -- Should not crash

-- Test 5: Test text functions
print("Test 5: Test text functions")
clear_text()
set_text_ink(255, 255, 255, 255)  -- White text
print_at(0, 0, "Hello from Lua test!")
print_at(0, 1, "Runtime is working correctly")

-- Test 6: Test text colors
print("Test 6: Test text colors")
set_text_ink(255, 0, 0, 255)      -- Red
print_at(0, 3, "Red text")
set_text_ink(0, 255, 0, 255)      -- Green
print_at(10, 3, "Green text")
set_text_ink(0, 0, 255, 255)      -- Blue
print_at(20, 3, "Blue text")

-- Test 7: Test cursor functions
print("Test 7: Test cursor functions")
set_cursor_position(0, 5)
set_cursor_visible(true)
set_cursor_type(1)  -- Block cursor
set_cursor_color(255, 255, 0, 255)  -- Yellow cursor

-- Test 8: Test graphics functions
print("Test 8: Test basic graphics")
set_draw_color(255, 0, 255, 255)  -- Magenta
draw_line(100, 100, 200, 150)
draw_rect(250, 100, 50, 30)
fill_rect(320, 100, 50, 30)
draw_circle(400, 115, 20)
fill_circle(450, 115, 15)

-- Test 9: Test input system initialization
print("Test 9: Test input system")
local input_init = init_input_system()
assert_true(input_init, "Input system should initialize")

-- Wait a moment to see the results
print("Test completed successfully!")
print("You should see colored text and graphics in the window")
sleep(2)  -- Wait 2 seconds

print("=== All Basic Runtime Tests PASSED ===")
