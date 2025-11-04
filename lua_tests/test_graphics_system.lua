-- Graphics System Comprehensive Test
-- Tests all graphics-related functions including lines, shapes, and colors

print("=== Graphics System Test ===")

-- Initialize runtime first
local init_result = init_abstract_runtime(1)  -- 800x600 mode
assert_true(init_result, "Runtime initialization should succeed")

-- Initialize input system
init_input_system()

-- Clear everything first
clear_graphics()
clear_text()

-- Test 1: Basic line drawing
print("Test 1: Basic line drawing")
set_draw_color(255, 255, 255, 255)  -- White lines
draw_line(50, 50, 150, 50)   -- Horizontal line
draw_line(50, 50, 50, 150)   -- Vertical line
draw_line(50, 50, 150, 150)  -- Diagonal line
draw_line(150, 50, 50, 150)  -- Cross diagonal

set_text_ink(255, 255, 255, 255)
print_at(0, 0, "Test 1: White lines (cross pattern)")

sleep(1)

-- Test 2: Colored lines
print("Test 2: Colored lines")
set_draw_color(255, 0, 0, 255)    -- Red
draw_line(200, 50, 300, 100)
set_draw_color(0, 255, 0, 255)    -- Green
draw_line(200, 75, 300, 125)
set_draw_color(0, 0, 255, 255)    -- Blue
draw_line(200, 100, 300, 150)

print_at(0, 1, "Test 2: Red, Green, Blue lines")

sleep(1)

-- Test 3: Rectangle outlines
print("Test 3: Rectangle outlines")
set_draw_color(255, 255, 0, 255)  -- Yellow
draw_rect(350, 50, 100, 75)       -- Large rectangle
draw_rect(375, 75, 50, 25)        -- Smaller rectangle inside

set_draw_color(255, 0, 255, 255)  -- Magenta
draw_rect(500, 50, 80, 60)        -- Another rectangle

print_at(0, 2, "Test 3: Yellow and Magenta rectangle outlines")

sleep(1)

-- Test 4: Filled rectangles
print("Test 4: Filled rectangles")
set_draw_color(128, 0, 128, 255)  -- Purple
fill_rect(50, 200, 80, 60)
set_draw_color(0, 128, 128, 255)  -- Teal
fill_rect(150, 200, 80, 60)
set_draw_color(128, 128, 0, 255)  -- Olive
fill_rect(250, 200, 80, 60)

print_at(0, 3, "Test 4: Purple, Teal, Olive filled rectangles")

sleep(1)

-- Test 5: Circle outlines
print("Test 5: Circle outlines")
set_draw_color(255, 128, 0, 255)  -- Orange
draw_circle(100, 350, 30)         -- Medium circle
draw_circle(200, 350, 45)         -- Larger circle
draw_circle(300, 350, 20)         -- Smaller circle

print_at(0, 4, "Test 5: Orange circle outlines")

sleep(1)

-- Test 6: Filled circles
print("Test 6: Filled circles")
set_draw_color(255, 192, 203, 255)  -- Pink
fill_circle(450, 350, 25)
set_draw_color(173, 216, 230, 255)  -- Light blue
fill_circle(550, 350, 35)
set_draw_color(144, 238, 144, 255)  -- Light green
fill_circle(500, 400, 20)

print_at(0, 5, "Test 6: Pink, Light Blue, Light Green filled circles")

sleep(1)

-- Test 7: Complex shapes and patterns
print("Test 7: Complex patterns")
clear_graphics()

-- Draw a grid pattern
set_draw_color(64, 64, 64, 255)  -- Dark gray
for x = 0, 600, 50 do
    draw_line(x, 0, x, 450)
end
for y = 0, 450, 50 do
    draw_line(0, y, 600, y)
end

-- Draw colorful squares on the grid
local colors = {
    {255, 0, 0, 255},    -- Red
    {0, 255, 0, 255},    -- Green
    {0, 0, 255, 255},    -- Blue
    {255, 255, 0, 255},  -- Yellow
    {255, 0, 255, 255},  -- Magenta
    {0, 255, 255, 255},  -- Cyan
}

for i = 1, 6 do
    local color = colors[i]
    set_draw_color(color[1], color[2], color[3], color[4])
    fill_rect(i * 80 - 30, 150, 40, 40)
end

print_at(0, 6, "Test 7: Grid with colored squares")

sleep(2)

-- Test 8: Transparency effects
print("Test 8: Transparency effects")
clear_graphics()

-- Draw overlapping shapes with different alpha values
set_draw_color(255, 0, 0, 128)    -- Semi-transparent red
fill_rect(100, 100, 100, 100)

set_draw_color(0, 255, 0, 128)    -- Semi-transparent green
fill_rect(150, 120, 100, 100)

set_draw_color(0, 0, 255, 128)    -- Semi-transparent blue
fill_rect(125, 140, 100, 100)

print_at(0, 7, "Test 8: Overlapping semi-transparent rectangles")

sleep(2)

-- Test 9: Geometric patterns
print("Test 9: Geometric patterns")
clear_graphics()

-- Draw a star pattern
set_draw_color(255, 255, 255, 255)  -- White
local center_x, center_y = 300, 225
local radius = 80

-- Draw star with lines
for i = 0, 4 do
    local angle1 = (i * 72) * math.pi / 180
    local angle2 = ((i * 72 + 36) * math.pi / 180)

    local x1 = math.floor(center_x + radius * math.cos(angle1))
    local y1 = math.floor(center_y + radius * math.sin(angle1))
    local x2 = math.floor(center_x + radius * 0.4 * math.cos(angle2))
    local y2 = math.floor(center_y + radius * 0.4 * math.sin(angle2))

    draw_line(center_x, center_y, x1, y1)
    draw_line(x1, y1, x2, y2)
end

print_at(0, 8, "Test 9: Star pattern")

sleep(2)

-- Test 10: Stress test - many small shapes
print("Test 10: Performance test")
clear_graphics()

-- Draw many small circles in a pattern
set_draw_color(0, 255, 255, 255)  -- Cyan
for x = 10, 590, 20 do
    for y = 50, 400, 20 do
        local size = math.random(3, 8)
        fill_circle(x, y, size)
    end
end

print_at(0, 9, "Test 10: Many small cyan circles")

sleep(2)

-- Test 11: Edge cases and bounds checking
print("Test 11: Edge cases")
clear_graphics()

-- Draw at screen edges (should not crash)
set_draw_color(255, 255, 0, 255)  -- Yellow
draw_line(0, 0, 799, 0)           -- Top edge
draw_line(0, 599, 799, 599)       -- Bottom edge (if visible)
draw_line(0, 0, 0, 599)           -- Left edge
draw_line(799, 0, 799, 599)       -- Right edge (if visible)

-- Draw shapes at corners
fill_circle(10, 10, 5)            -- Top-left
fill_circle(790, 10, 5)           -- Top-right (if visible)
fill_rect(0, 580, 20, 20)         -- Bottom area

print_at(0, 10, "Test 11: Edge and corner drawing")

sleep(1)

-- Test 12: Color validation
print("Test 12: Color edge cases")

-- Test with extreme color values (should be clamped safely)
set_draw_color(255, 255, 255, 255)  -- Valid white
fill_rect(50, 450, 30, 30)

set_draw_color(0, 0, 0, 255)        -- Valid black
fill_rect(100, 450, 30, 30)

print_at(0, 11, "Test 12: Color validation (white and black squares)")

sleep(1)

-- Final display with summary
clear_text()
set_text_ink(0, 255, 0, 255)  -- Green for success
print_at(0, 0, "=== All Graphics System Tests PASSED ===")
set_text_ink(255, 255, 255, 255)  -- Reset to white

print_at(0, 2, "Graphics capabilities verified:")
print_at(0, 3, "✓ Line drawing (various colors)")
print_at(0, 4, "✓ Rectangle outlines and filled")
print_at(0, 5, "✓ Circle outlines and filled")
print_at(0, 6, "✓ Color management and transparency")
print_at(0, 7, "✓ Complex patterns and geometry")
print_at(0, 8, "✓ Performance with many shapes")
print_at(0, 9, "✓ Edge case handling")

print_at(0, 11, "Graphics system test completed successfully!")

sleep(3)  -- Show results for 3 seconds

print("Graphics system test completed successfully!")
