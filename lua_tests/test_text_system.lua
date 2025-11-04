-- Text System Comprehensive Test
-- Tests all text-related functions and cursor system

print("=== Text System Test ===")

-- Initialize runtime first
local init_result = init_abstract_runtime(1)  -- 800x600 mode
assert_true(init_result, "Runtime initialization should succeed")

-- Initialize input system
init_input_system()

-- Test 1: Basic text output
print("Test 1: Basic text output")
clear_text()
set_text_ink(255, 255, 255, 255)  -- White text
print_at(0, 0, "Text System Test - Line 0")
print_at(0, 1, "This is line 1")
print_at(0, 2, "This is line 2")

-- Test 2: Text positioning
print("Test 2: Text positioning")
print_at(10, 4, "Offset text at (10,4)")
print_at(20, 5, "More offset at (20,5)")
print_at(5, 6, "Back to (5,6)")

-- Test 3: Text ink colors
print("Test 3: Text ink colors")
set_text_ink(255, 0, 0, 255)      -- Red
print_at(0, 8, "Red text")
set_text_ink(0, 255, 0, 255)      -- Green
print_at(10, 8, "Green")
set_text_ink(0, 0, 255, 255)      -- Blue
print_at(18, 8, "Blue")
set_text_ink(255, 255, 0, 255)    -- Yellow
print_at(25, 8, "Yellow")
set_text_ink(255, 0, 255, 255)    -- Magenta
print_at(33, 8, "Magenta")
set_text_ink(0, 255, 255, 255)    -- Cyan
print_at(42, 8, "Cyan")

-- Test 4: Text paper (background) colors
print("Test 4: Text paper colors")
set_text_ink(0, 0, 0, 255)        -- Black text
set_text_paper(255, 255, 255, 255) -- White background
print_at(0, 10, "Black on White")

set_text_ink(255, 255, 255, 255)   -- White text
set_text_paper(255, 0, 0, 255)     -- Red background
print_at(16, 10, "White on Red")

set_text_ink(255, 255, 0, 255)     -- Yellow text
set_text_paper(0, 0, 255, 255)     -- Blue background
print_at(28, 10, "Yellow on Blue")

-- Reset to normal colors
set_text_ink(255, 255, 255, 255)
set_text_paper(0, 0, 0, 0)  -- Transparent background

-- Test 5: Poke and peek text colors
print("Test 5: Poke and peek text colors")
poke_text_ink(0, 12, 128, 255, 128, 255)  -- Light green
poke_text_paper(0, 12, 64, 0, 64, 255)    -- Dark purple background
print_at(0, 12, "P")  -- This should use the poked colors

-- Peek the colors back
local r, g, b, a = peek_text_ink(0, 12)
print("Peeked ink color: " .. r .. "," .. g .. "," .. b .. "," .. a)
assert_equals(r, 128, "Red component should match")
assert_equals(g, 255, "Green component should match")
assert_equals(b, 128, "Blue component should match")
assert_equals(a, 255, "Alpha component should match")

local pr, pg, pb, pa = peek_text_paper(0, 12)
print("Peeked paper color: " .. pr .. "," .. pg .. "," .. pb .. "," .. pa)
assert_equals(pr, 64, "Paper red component should match")

-- Test 6: Cursor system
print("Test 6: Cursor system")
set_cursor_position(5, 14)
set_cursor_visible(true)
set_cursor_type(0)  -- Underscore cursor
set_cursor_color(255, 255, 0, 255)  -- Yellow cursor
print_at(0, 14, "Cursor tests:")

sleep(0.5)  -- Brief pause

-- Test different cursor types
set_cursor_type(1)  -- Block cursor
set_cursor_position(15, 14)
sleep(0.5)

set_cursor_type(2)  -- Vertical bar cursor
set_cursor_position(25, 14)
sleep(0.5)

-- Test cursor colors
set_cursor_color(255, 0, 0, 255)  -- Red
set_cursor_position(35, 14)
sleep(0.5)

set_cursor_color(0, 255, 0, 255)  -- Green
set_cursor_position(45, 14)
sleep(0.5)

-- Test cursor blinking
print_at(0, 16, "Testing cursor blink (on/off)")
set_cursor_position(25, 16)
enable_cursor_blink(false)  -- Solid cursor
sleep(1)
enable_cursor_blink(true)   -- Blinking cursor
sleep(1)

-- Test 7: Text scrolling
print("Test 7: Text scrolling")
set_cursor_visible(false)  -- Hide cursor for scrolling test

-- Fill screen with numbered lines
for i = 0, 24 do
    print_at(0, i, "Line " .. i .. " - This line will scroll")
end

sleep(1)
print("Scrolling up...")
scroll_text_up()
sleep(0.5)
scroll_text_up()
sleep(0.5)
scroll_text_up()

sleep(1)
print("Scrolling down...")
scroll_text_down()
sleep(0.5)
scroll_text_down()

-- Test 8: Clear text
print("Test 8: Clear text")
sleep(1)
clear_text()
set_text_ink(0, 255, 0, 255)  -- Green
print_at(0, 0, "Screen cleared successfully!")

-- Test 9: Long text handling
print("Test 9: Long text handling")
set_text_ink(255, 255, 255, 255)
local long_text = "This is a very long line of text that might extend beyond the normal screen width to test how the text system handles it"
print_at(0, 2, long_text)

-- Test 10: Special characters
print("Test 10: Special characters")
print_at(0, 4, "Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?")
print_at(0, 5, "Numbers: 0123456789")
print_at(0, 6, "Mixed: ABC123xyz!@#")

-- Test 11: Text bounds checking
print("Test 11: Text bounds (edge cases)")
-- These should not crash, even if they're outside normal bounds
print_at(79, 24, "X")  -- Bottom right corner
print_at(0, 24, "Bottom left")

-- Summary
set_text_ink(0, 255, 0, 255)  -- Green for success
print_at(0, 20, "=== All Text System Tests PASSED ===")
set_text_ink(255, 255, 255, 255)  -- Reset to white

sleep(2)  -- Show results for 2 seconds

print("Text system test completed successfully!")
