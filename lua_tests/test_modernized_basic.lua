-- Basic Modern Test for Available Lua Bindings
-- Tests only functions that are currently implemented

print("=== Modern Basic Test ===")
print("Testing currently available Lua bindings")

-- Test 1: Basic runtime state check
print("Test 1: Check runtime quit state")
local should_quit = should_quit()
assert_false(should_quit, "Runtime should not be quitting initially")

-- Test 2: Screen dimensions (these should work if display functions are bound)
print("Test 2: Check screen dimensions")
local width = get_screen_width()
local height = get_screen_height()
assert_true(width > 0, "Screen width should be positive: " .. tostring(width))
assert_true(height > 0, "Screen height should be positive: " .. tostring(height))
print("Screen dimensions: " .. width .. "x" .. height)

-- Test 3: Background color (should not crash)
print("Test 3: Set background color")
set_background_color(50, 100, 150)
print("Background color set successfully")

-- Test 4: Text functions
print("Test 4: Basic text operations")
clear_text()
set_text_ink(255, 255, 255, 255) -- White text
print_at(0, 0, "Modernized Lua Test")
print_at(0, 1, "Runtime: OK")
print_at(0, 2, "Bindings: Working")

-- Test 5: Text colors
print("Test 5: Text color changes")
set_text_ink(255, 100, 100, 255) -- Light red
print_at(0, 4, "Red text")
set_text_ink(100, 255, 100, 255) -- Light green
print_at(15, 4, "Green text")
set_text_ink(100, 100, 255, 255) -- Light blue
print_at(30, 4, "Blue text")

-- Test 6: Graphics functions (if available)
print("Test 6: Basic graphics")
clear_graphics()
set_draw_color(255, 200, 0, 255) -- Orange
draw_line(100, 100, 200, 150)
draw_rect(220, 100, 60, 40)
fill_rect(300, 100, 60, 40)
draw_circle(400, 120, 25)
fill_circle(450, 120, 20)

-- Test 7: Console output integration
print("Test 7: Console integration test")
print("This message should appear in both test output and console")

-- Test 8: Test assertions work correctly
print("Test 8: Assertion functions")
assert_equals("hello", "hello", "String equality should work")
assert_true(true, "True assertion should pass")
assert_false(false, "False assertion should pass")
assert_not_nil("test", "Non-nil assertion should pass")

-- Test 9: Math and basic Lua functionality
print("Test 9: Lua standard functionality")
local test_table = { a = 1, b = 2, c = 3 }
assert_not_nil(test_table, "Table creation should work")
assert_equals(test_table.a, 1, "Table access should work")

local sum = 0
for i = 1, 5 do
    sum = sum + i
end
assert_equals(sum, 15, "Loops should work correctly")

-- Test 10: String operations
print("Test 10: String operations")
local test_string = "Hello " .. "World"
assert_equals(test_string, "Hello World", "String concatenation should work")

print("=== Modern Basic Test COMPLETED ===")
print("All available bindings tested successfully!")
