-- Test color functionality in the abstract runtime
-- This test checks if set_draw_color works properly and colors are displayed correctly

print("=== Color Functionality Test ===")

-- Set dark background for better color visibility
set_background_color(0, 0, 32)
clear_graphics()

-- Test 1: Red rectangle
print("Drawing red rectangle...")
set_draw_color(255, 0, 0, 255)  -- Red
fill_rect(50, 50, 100, 50)

-- Test 2: Green rectangle
print("Drawing green rectangle...")
set_draw_color(0, 255, 0, 255)  -- Green
fill_rect(50, 120, 100, 50)

-- Test 3: Blue rectangle
print("Drawing blue rectangle...")
set_draw_color(0, 0, 255, 255)  -- Blue
fill_rect(50, 190, 100, 50)

-- Test 4: Yellow circle
print("Drawing yellow circle...")
set_draw_color(255, 255, 0, 255)  -- Yellow
fill_circle(300, 150, 40)

-- Test 5: Cyan line
print("Drawing cyan line...")
set_draw_color(0, 255, 255, 255)  -- Cyan
draw_line(200, 50, 350, 200)

-- Test 6: Magenta outline rectangle
print("Drawing magenta outline rectangle...")
set_draw_color(255, 0, 255, 255)  -- Magenta
draw_rect(400, 100, 80, 60)

-- Test 7: Semi-transparent orange circle
print("Drawing semi-transparent orange circle...")
set_draw_color(255, 165, 0, 128)  -- Orange with 50% transparency
fill_circle(400, 200, 30)

-- Test 8: White text labels
set_text_ink(255, 255, 255, 255)  -- White text
print_at(1, 1, "Color Test - Check if colors appear correctly:")
print_at(1, 3, "Red rectangle (top left)")
print_at(1, 4, "Green rectangle (middle left)")
print_at(1, 5, "Blue rectangle (bottom left)")
print_at(1, 6, "Yellow circle (center)")
print_at(1, 7, "Cyan diagonal line")
print_at(1, 8, "Magenta rectangle outline (right)")
print_at(1, 9, "Semi-transparent orange circle (bottom right)")

-- Test 9: Color gradients using lines
print("Drawing color gradient...")
for i = 0, 99 do
    local red = math.floor(255 * (i / 99))
    local blue = math.floor(255 * (1 - i / 99))
    set_draw_color(red, 0, blue, 255)
    draw_line(500 + i, 50, 500 + i, 100)
end

print_at(1, 11, "Red-to-Blue gradient (top right)")

-- Test color persistence - draw multiple shapes with same color
print("Testing color persistence...")
set_draw_color(128, 255, 128, 255)  -- Light green
fill_rect(200, 250, 30, 30)
fill_rect(240, 250, 30, 30)
fill_rect(280, 250, 30, 30)

print_at(1, 13, "Three light green squares should be same color")

-- Final verification text
print_at(1, 15, "If all shapes appear in their correct colors,")
print_at(1, 16, "then set_draw_color is working properly!")
print_at(1, 18, "Press any key to continue...")

-- Wait for user input
while true do
    local key_pressed = key()
    if key_pressed ~= 0 then
        break
    end
    sleep(50)  -- Sleep 50ms to avoid busy waiting
end

print("Color functionality test completed!")
