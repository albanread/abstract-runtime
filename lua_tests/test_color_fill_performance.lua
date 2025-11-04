-- Color Fill Performance Test
-- Demonstrates the need for a fill_text_color function for efficient bulk color operations
-- This test shows the performance difference between individual poke operations vs bulk operations

print("=== Color Fill Performance Test ===")
print("Testing the need for efficient bulk color operations")

console_section("Color Fill Performance Analysis")
console_info("Measuring performance of individual vs bulk color operations")

-- Get screen dimensions
local screen_width = get_screen_width()
local screen_height = get_screen_height()

-- Calculate text grid dimensions
local char_width = 8
local char_height = 16
local text_cols = math.floor(screen_width / char_width)
local text_rows = math.floor(screen_height / char_height)

console_info("Screen: " .. screen_width .. "x" .. screen_height .. " pixels")
console_info("Text grid: " .. text_cols .. "x" .. text_rows .. " characters")
console_info("Total characters: " .. (text_cols * text_rows))

-- Test 1: Individual poke_text_ink calls (current method)
print("Test 1: Individual poke_text_ink calls")
console_info("Test 1: Measuring individual poke_text_ink performance")

clear_text()

local start_time = os.clock()

-- Fill screen with red text color using individual poke calls
for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        poke_text_ink(col, row, 255, 100, 100, 255) -- Red
    end
end

local individual_poke_time = os.clock() - start_time
local total_chars = text_cols * text_rows

console_info("Individual poke time: " .. string.format("%.3f", individual_poke_time) .. " seconds")
console_info("Pokes per second: " .. string.format("%.0f", total_chars / individual_poke_time))

-- Test 2: Individual poke_text_paper calls (background)
print("Test 2: Individual poke_text_paper calls")
console_info("Test 2: Measuring individual poke_text_paper performance")

start_time = os.clock()

-- Fill screen with blue background color using individual poke calls
for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        poke_text_paper(col, row, 100, 100, 255, 255) -- Blue background
    end
end

local individual_paper_time = os.clock() - start_time

console_info("Individual paper poke time: " .. string.format("%.3f", individual_paper_time) .. " seconds")
console_info("Paper pokes per second: " .. string.format("%.0f", total_chars / individual_paper_time))

-- Test 3: Combined ink and paper poke operations
print("Test 3: Combined ink and paper operations")
console_info("Test 3: Measuring combined color operations")

start_time = os.clock()

-- Fill screen with both foreground and background colors
for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        poke_text_ink(col, row, 255, 255, 100, 255)  -- Yellow text
        poke_text_paper(col, row, 100, 50, 150, 255) -- Purple background
    end
end

local combined_poke_time = os.clock() - start_time

console_info("Combined poke time: " .. string.format("%.3f", combined_poke_time) .. " seconds")
console_info("Combined operations per second: " .. string.format("%.0f", (total_chars * 2) / combined_poke_time))

-- Test 4: Regional color fills (quarters)
print("Test 4: Regional color operations")
console_info("Test 4: Measuring regional color fills")

local quarter_cols = math.floor(text_cols / 2)
local quarter_rows = math.floor(text_rows / 2)

start_time = os.clock()

-- Fill four quarters with different colors
-- Top-left: Red on Black
for row = 0, quarter_rows - 1 do
    for col = 0, quarter_cols - 1 do
        poke_text_ink(col, row, 255, 100, 100, 255)
        poke_text_paper(col, row, 0, 0, 0, 255)
    end
end

-- Top-right: Green on Dark Blue
for row = 0, quarter_rows - 1 do
    for col = quarter_cols, text_cols - 1 do
        poke_text_ink(col, row, 100, 255, 100, 255)
        poke_text_paper(col, row, 0, 0, 100, 255)
    end
end

-- Bottom-left: Blue on Dark Red
for row = quarter_rows, text_rows - 1 do
    for col = 0, quarter_cols - 1 do
        poke_text_ink(col, row, 100, 100, 255, 255)
        poke_text_paper(col, row, 100, 0, 0, 255)
    end
end

-- Bottom-right: Yellow on Dark Green
for row = quarter_rows, text_rows - 1 do
    for col = quarter_cols, text_cols - 1 do
        poke_text_ink(col, row, 255, 255, 100, 255)
        poke_text_paper(col, row, 0, 100, 0, 255)
    end
end

local regional_fill_time = os.clock() - start_time

console_info("Regional fill time: " .. string.format("%.3f", regional_fill_time) .. " seconds")
console_info("Regional operations per second: " .. string.format("%.0f", (total_chars * 2) / regional_fill_time))

-- Test 5: Line-by-line color patterns
print("Test 5: Line patterns")
console_info("Test 5: Measuring line-by-line color patterns")

start_time = os.clock()

-- Create alternating line colors
for row = 0, text_rows - 1 do
    local ink_color = { 255, 255, 255, 255 }
    local paper_color = { 0, 0, 0, 255 }

    if row % 2 == 0 then
        ink_color = { 0, 0, 0, 255 }
        paper_color = { 255, 255, 255, 255 }
    end

    for col = 0, text_cols - 1 do
        poke_text_ink(col, row, ink_color[1], ink_color[2], ink_color[3], ink_color[4])
        poke_text_paper(col, row, paper_color[1], paper_color[2], paper_color[3], paper_color[4])
    end
end

local line_pattern_time = os.clock() - start_time

console_info("Line pattern time: " .. string.format("%.3f", line_pattern_time) .. " seconds")
console_info("Line pattern operations per second: " .. string.format("%.0f", (total_chars * 2) / line_pattern_time))

-- Performance Analysis
console_section("Performance Analysis and Optimization Needs")

console_info("Current Performance Results:")
console_info("Individual ink pokes:  " .. string.format("%8.0f", total_chars / individual_poke_time) .. " ops/sec")
console_info("Individual paper pokes:" .. string.format("%8.0f", total_chars / individual_paper_time) .. " ops/sec")
console_info("Combined operations:   " .. string.format("%8.0f", (total_chars * 2) / combined_poke_time) .. " ops/sec")
console_info("Regional fills:        " .. string.format("%8.0f", (total_chars * 2) / regional_fill_time) .. " ops/sec")
console_info("Line patterns:         " .. string.format("%8.0f", (total_chars * 2) / line_pattern_time) .. " ops/sec")

console_separator()

-- Calculate total time for full screen color changes
local full_screen_time = individual_poke_time + individual_paper_time
console_info("Current full screen color fill time: " .. string.format("%.3f", full_screen_time) .. " seconds")

-- Estimate improvement with bulk operations
local estimated_improvement = 10 -- Conservative estimate: 10x faster with memset
local estimated_bulk_time = full_screen_time / estimated_improvement

console_info("Estimated bulk operation time: " .. string.format("%.3f", estimated_bulk_time) .. " seconds")
console_info("Potential speedup: " .. string.format("%.1f", estimated_improvement) .. "x faster")

console_section("Proposed fill_text_color Function")
console_info("Need for efficient bulk color operations:")
console_info("1. Current method requires " .. string.format("%.0f", total_chars * 2) .. " individual function calls")
console_info("2. Each call has overhead for bounds checking and updates")
console_info("3. Memory access pattern is not cache-friendly")
console_info("4. No SIMD or vectorization opportunities")

console_separator()
console_info("Proposed function signature:")
console_info("fill_text_color(x, y, width, height, ink_r, ink_g, ink_b, ink_a, paper_r, paper_g, paper_b, paper_a)")
console_info("")
console_info("Benefits:")
console_info("- Single function call for rectangular regions")
console_info("- Can use memset/memcpy for bulk operations")
console_info("- Better cache locality and memory access patterns")
console_info("- Reduced function call overhead")
console_info("- Opportunity for SIMD optimizations")

console_separator()
console_info("Use cases that would benefit:")
console_info("- Full screen color initialization")
console_info("- Window/dialog background setup")
console_info("- UI element styling")
console_info("- Game area background preparation")
console_info("- Text editor syntax highlighting setup")
console_info("- Console/terminal emulation")

-- Test 6: Demonstrate typical use cases
print("Test 6: Typical use case scenarios")
console_section("Real-World Use Case Performance")

-- Use case 1: Dialog box background
local dialog_x = 20
local dialog_y = 10
local dialog_w = 40
local dialog_h = 15

start_time = os.clock()

-- Current method: individual pokes for dialog
for row = dialog_y, dialog_y + dialog_h - 1 do
    for col = dialog_x, dialog_x + dialog_w - 1 do
        poke_text_ink(col, row, 0, 0, 0, 255)         -- Black text
        poke_text_paper(col, row, 200, 200, 200, 255) -- Light gray background
    end
end

local dialog_time = os.clock() - start_time
local dialog_cells = dialog_w * dialog_h

console_info("Dialog box (" .. dialog_cells .. " cells): " ..
    string.format("%.6f", dialog_time) .. "s (" ..
    string.format("%.0f", (dialog_cells * 2) / dialog_time) .. " ops/sec)")

-- Use case 2: Status bar
local status_y = text_rows - 1
start_time = os.clock()

for col = 0, text_cols - 1 do
    poke_text_ink(col, status_y, 255, 255, 255, 255) -- White text
    poke_text_paper(col, status_y, 0, 100, 200, 255) -- Blue background
end

local status_time = os.clock() - start_time

console_info("Status bar (" .. text_cols .. " cells): " ..
    string.format("%.6f", status_time) .. "s (" ..
    string.format("%.0f", (text_cols * 2) / status_time) .. " ops/sec)")

-- Use case 3: Game area borders
local border_thickness = 2
start_time = os.clock()

-- Top border
for row = 0, border_thickness - 1 do
    for col = 0, text_cols - 1 do
        poke_text_ink(col, row, 255, 255, 0, 255) -- Yellow
        poke_text_paper(col, row, 100, 0, 0, 255) -- Dark red
    end
end

-- Bottom border
for row = text_rows - border_thickness, text_rows - 1 do
    for col = 0, text_cols - 1 do
        poke_text_ink(col, row, 255, 255, 0, 255)
        poke_text_paper(col, row, 100, 0, 0, 255)
    end
end

-- Left border
for row = 0, text_rows - 1 do
    for col = 0, border_thickness - 1 do
        poke_text_ink(col, row, 255, 255, 0, 255)
        poke_text_paper(col, row, 100, 0, 0, 255)
    end
end

-- Right border
for row = 0, text_rows - 1 do
    for col = text_cols - border_thickness, text_cols - 1 do
        poke_text_ink(col, row, 255, 255, 0, 255)
        poke_text_paper(col, row, 100, 0, 0, 255)
    end
end

local border_time = os.clock() - start_time
local border_cells = (text_cols * border_thickness * 2) + (text_rows * border_thickness * 2) -
(border_thickness * border_thickness * 4)

console_info("Game borders (" .. border_cells .. " cells): " ..
    string.format("%.6f", border_time) .. "s (" ..
    string.format("%.0f", (border_cells * 2) / border_time) .. " ops/sec)")

-- Final recommendations
console_section("Implementation Recommendations")
console_info("Priority functions to implement:")
console_info("1. fill_text_color(x, y, w, h, ink_r, ink_g, ink_b, ink_a, paper_r, paper_g, paper_b, paper_a)")
console_info("2. fill_text_ink(x, y, w, h, r, g, b, a) - foreground only")
console_info("3. fill_text_paper(x, y, w, h, r, g, b, a) - background only")
console_info("4. clear_text_colors() - reset entire screen to default colors")

console_separator()
console_info("Expected performance improvements:")
console_info("- Full screen operations: 5-20x faster")
console_info("- Regional fills: 3-10x faster")
console_info("- Dialog/UI setup: 2-5x faster")
console_info("- Real-time color effects become practical")

console_info("This would enable:")
console_info("- Smooth color animations")
console_info("- Efficient UI rendering")
console_info("- Real-time syntax highlighting")
console_info("- Game background effects")
console_info("- Professional text-mode applications")

print("=== Color Fill Performance Test COMPLETED ===")
console_info("Performance analysis completed - bulk color operations strongly recommended!")
console_separator()
