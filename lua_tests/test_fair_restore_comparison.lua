-- Fair Restore Comparison Test
-- Compares equivalent operations: pure text filling vs restore of same content
-- This eliminates the complexity of create_complex_screen() and measures like-for-like

print("=== Fair Restore Comparison Test ===")
print("Comparing equivalent text operations vs restore for fair measurement")

console_section("Fair Performance Comparison")
console_info("Testing equivalent operations to get accurate performance data")

-- Get screen dimensions
local screen_width = get_screen_width()
local screen_height = get_screen_height()
local char_width = 8
local char_height = 16
local text_cols = math.floor(screen_width / char_width)
local text_rows = math.floor(screen_height / char_height)

console_info("Screen: " .. screen_width .. "x" .. screen_height .. " pixels")
console_info("Text grid: " .. text_cols .. "x" .. text_rows .. " characters")

local ITERATIONS = 30

-- Test 1: Simple screen fill with print_at only
print("Test 1: Simple text-only screen creation")
console_info("Test 1: Creating screen with pure print_at operations")

local function create_simple_text_screen()
    clear_text()

    -- Fill screen with text pattern using only print_at (no color operations)
    for row = 0, text_rows - 1 do
        local line = ""
        for col = 0, text_cols - 1, 8 do
            if col + 7 < text_cols then
                line = line .. string.format("%02d:%02d ", row, col)
            end
        end
        if #line > 0 then
            print_at(0, row, line:sub(1, text_cols))
        end
    end
end

-- Measure command queuing time for simple screen
local simple_command_times = {}
local total_simple_command_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_simple_text_screen()
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    simple_command_times[i] = iteration_time
    total_simple_command_time = total_simple_command_time + iteration_time
end

local avg_simple_command = total_simple_command_time / ITERATIONS
console_info("Simple text creation (command): " .. string.format("%.6f", avg_simple_command) .. "s average")

-- Measure actual render time for simple screen
local simple_render_times = {}
local total_simple_render_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_simple_text_screen()
    wait_for_render_complete()
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    simple_render_times[i] = iteration_time
    total_simple_render_time = total_simple_render_time + iteration_time
end

local avg_simple_render = total_simple_render_time / ITERATIONS
console_info("Simple text creation (actual): " .. string.format("%.6f", avg_simple_render) .. "s average")

-- Test 2: Save the simple screen for restore testing
print("Test 2: Save reference screen")
create_simple_text_screen()
save_text(0)
console_info("Simple text screen saved to slot 0")

-- Test 3: Restore command queuing time
print("Test 3: Restore command queuing")
local restore_command_times = {}
local total_restore_command_time = 0

for i = 1, ITERATIONS do
    clear_text()

    local start_time = os.clock()
    restore_text(0)
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    restore_command_times[i] = iteration_time
    total_restore_command_time = total_restore_command_time + iteration_time
end

local avg_restore_command = total_restore_command_time / ITERATIONS
console_info("Restore text (command): " .. string.format("%.6f", avg_restore_command) .. "s average")

-- Test 4: Restore actual render time
print("Test 4: Restore actual render time")
local restore_render_times = {}
local total_restore_render_time = 0

for i = 1, ITERATIONS do
    clear_text()

    local start_time = os.clock()
    restore_text(0)
    wait_for_render_complete()
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    restore_render_times[i] = iteration_time
    total_restore_render_time = total_restore_render_time + iteration_time
end

local avg_restore_render = total_restore_render_time / ITERATIONS
console_info("Restore text (actual): " .. string.format("%.6f", avg_restore_render) .. "s average")

-- Test 5: Color-only operations test
print("Test 5: Color-only operations")
console_info("Test 5: Testing pure color operations vs restore")

local function create_color_only_screen()
    clear_text()
    -- Only color operations, no text
    fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, 100, 100, 100, 255)
end

-- Save color screen
create_color_only_screen()
save_text(1)

-- Measure color creation
local color_create_times = {}
local total_color_create_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_color_only_screen()
    wait_for_render_complete()
    local end_time = os.clock()

    color_create_times[i] = end_time - start_time
    total_color_create_time = total_color_create_time + color_create_times[i]
end

local avg_color_create = total_color_create_time / ITERATIONS

-- Measure color restore
local color_restore_times = {}
local total_color_restore_time = 0

for i = 1, ITERATIONS do
    clear_text()

    local start_time = os.clock()
    restore_text(1)
    wait_for_render_complete()
    local end_time = os.clock()

    color_restore_times[i] = end_time - start_time
    total_color_restore_time = total_color_restore_time + color_restore_times[i]
end

local avg_color_restore = total_color_restore_time / ITERATIONS

console_info("Color creation (actual): " .. string.format("%.6f", avg_color_create) .. "s average")
console_info("Color restore (actual): " .. string.format("%.6f", avg_color_restore) .. "s average")

-- Test 6: Mixed content test (most realistic)
print("Test 6: Mixed content test")
console_info("Test 6: Mixed text and color operations")

local function create_mixed_screen()
    clear_text()
    -- Set background
    fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, 50, 50, 150, 255)
    -- Add some text
    print_at(10, 5, "Mixed Content Screen")
    print_at(10, 6, "Text with colored background")
    print_at(10, 7, "Line 3 of content")
end

-- Save mixed screen
create_mixed_screen()
save_text(2)

-- Measure mixed creation
local mixed_create_times = {}
local total_mixed_create_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_mixed_screen()
    wait_for_render_complete()
    local end_time = os.clock()

    mixed_create_times[i] = end_time - start_time
    total_mixed_create_time = total_mixed_create_time + mixed_create_times[i]
end

local avg_mixed_create = total_mixed_create_time / ITERATIONS

-- Measure mixed restore
local mixed_restore_times = {}
local total_mixed_restore_time = 0

for i = 1, ITERATIONS do
    clear_text()

    local start_time = os.clock()
    restore_text(2)
    wait_for_render_complete()
    local end_time = os.clock()

    mixed_restore_times[i] = end_time - start_time
    total_mixed_restore_time = total_mixed_restore_time + mixed_restore_times[i]
end

local avg_mixed_restore = total_mixed_restore_time / ITERATIONS

console_info("Mixed creation (actual): " .. string.format("%.6f", avg_mixed_create) .. "s average")
console_info("Mixed restore (actual): " .. string.format("%.6f", avg_mixed_restore) .. "s average")

-- Performance Analysis
console_section("Fair Comparison Results")

console_info("=== COMMAND QUEUING vs ACTUAL RENDERING ===")
console_info("Simple text operations:")
console_info("  Command queuing: " .. string.format("%.6f", avg_simple_command) .. "s")
console_info("  Actual rendering: " .. string.format("%.6f", avg_simple_render) .. "s")
console_info("  Rendering overhead: " .. string.format("%.1f", avg_simple_render / avg_simple_command) .. "x")

console_info("Restore operations:")
console_info("  Command queuing: " .. string.format("%.6f", avg_restore_command) .. "s")
console_info("  Actual rendering: " .. string.format("%.6f", avg_restore_render) .. "s")
console_info("  Rendering overhead: " .. string.format("%.1f", avg_restore_render / avg_restore_command) .. "x")

console_separator()
console_info("=== FAIR PERFORMANCE COMPARISON ===")

-- Text-only comparison
local text_speedup = avg_simple_render / avg_restore_render
console_info("Text-only operations:")
console_info("  Create: " .. string.format("%.6f", avg_simple_render) .. "s")
console_info("  Restore: " .. string.format("%.6f", avg_restore_render) .. "s")
if text_speedup > 1 then
    console_info("  Restore is " .. string.format("%.1f", text_speedup) .. "x FASTER")
else
    console_info("  Create is " .. string.format("%.1f", 1 / text_speedup) .. "x FASTER")
end

-- Color-only comparison
local color_speedup = avg_color_create / avg_color_restore
console_info("Color-only operations:")
console_info("  Create: " .. string.format("%.6f", avg_color_create) .. "s")
console_info("  Restore: " .. string.format("%.6f", avg_color_restore) .. "s")
if color_speedup > 1 then
    console_info("  Restore is " .. string.format("%.1f", color_speedup) .. "x FASTER")
else
    console_info("  Create is " .. string.format("%.1f", 1 / color_speedup) .. "x FASTER")
end

-- Mixed content comparison
local mixed_speedup = avg_mixed_create / avg_mixed_restore
console_info("Mixed content operations:")
console_info("  Create: " .. string.format("%.6f", avg_mixed_create) .. "s")
console_info("  Restore: " .. string.format("%.6f", avg_mixed_restore) .. "s")
if mixed_speedup > 1 then
    console_info("  Restore is " .. string.format("%.1f", mixed_speedup) .. "x FASTER")
else
    console_info("  Create is " .. string.format("%.1f", 1 / mixed_speedup) .. "x FASTER")
end

console_separator()
console_info("=== ANALYSIS ===")

-- Determine if restore is consistently better
local restore_wins = 0
local create_wins = 0

if text_speedup > 1 then restore_wins = restore_wins + 1 else create_wins = create_wins + 1 end
if color_speedup > 1 then restore_wins = restore_wins + 1 else create_wins = create_wins + 1 end
if mixed_speedup > 1 then restore_wins = restore_wins + 1 else create_wins = create_wins + 1 end

if restore_wins > create_wins then
    console_info("🚀 CONCLUSION: Restore operations are consistently faster")
    console_info("   The memcpy optimization worked!")
    console_info("   Bulk memory operations beat individual function calls")
elseif create_wins > restore_wins then
    console_info("🤔 CONCLUSION: Create operations are consistently faster")
    console_info("   This suggests there may be overhead in the restore path")
    console_info("   or the rendering pipeline treats them differently")
else
    console_info("⚖️  CONCLUSION: Performance is roughly equivalent")
    console_info("   Choice should be based on use case, not performance")
end

-- Calculate average speedups
local avg_speedup = (text_speedup + color_speedup + mixed_speedup) / 3
if avg_speedup > 1 then
    console_info("Average speedup: " .. string.format("%.1f", avg_speedup) .. "x faster with restore")
else
    console_info("Average speedup: " .. string.format("%.1f", 1 / avg_speedup) .. "x faster with create")
end

console_separator()
console_info("=== RECOMMENDATIONS ===")

if restore_wins >= 2 then
    console_info("✅ RECOMMENDED: Use save/restore for repeated content")
    console_info("   - Dialog systems")
    console_info("   - Screen templates")
    console_info("   - Multi-screen applications")
    console_info("   - Undo/redo systems")
else
    console_info("💡 SITUATIONAL: Choose based on use case")
    console_info("   - Use restore for: Exact screen repetition")
    console_info("   - Use create for: Dynamic/changing content")
    console_info("   - Performance difference is minimal")
end

-- Final visual confirmation
clear_text()
fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, 0, 100, 0, 255)
print_at(math.floor(text_cols / 2) - 15, math.floor(text_rows / 2) - 2, "FAIR COMPARISON COMPLETE")

if restore_wins > create_wins then
    print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2),
        "Restore wins: " .. string.format("%.1f", avg_speedup) .. "x average speedup")
else
    print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2),
        "Create wins: " .. string.format("%.1f", 1 / avg_speedup) .. "x average speedup")
end

print_at(math.floor(text_cols / 2) - 18, math.floor(text_rows / 2) + 2, "Accurate measurement with memcpy optimization")

console_info("=== TEST COMPLETE ===")
console_info("Fair comparison completed with optimized restore_text implementation")

print("=== Fair Restore Comparison Test COMPLETED ===")
console_separator()
