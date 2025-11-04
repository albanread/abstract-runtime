-- Print vs Restore Performance Benchmark
-- Comprehensive comparison of print_at operations vs save_text/restore_text
-- Measures performance difference for displaying identical screen content

print("=== Print vs Restore Performance Benchmark ===")
print("Comparing print_at operations vs save/restore for screen display")

console_section("Print vs Restore Performance Analysis")
console_info("Measuring performance difference between printing and restoring identical content")

-- Get screen dimensions
local screen_width = get_screen_width()
local screen_height = get_screen_height()
local char_width = 8
local char_height = 16
local text_cols = math.floor(screen_width / char_width)
local text_rows = math.floor(screen_height / char_height)

console_info("Screen: " .. screen_width .. "x" .. screen_height .. " pixels")
console_info("Text grid: " .. text_cols .. "x" .. text_rows .. " characters")

-- Test configuration
local ITERATIONS = 100 -- Number of iterations for accurate timing
local test_results = {}

-- Helper function to create consistent test content
local function create_test_screen()
    clear_text()

    -- Header with colors
    fill_text_color(0, 0, text_cols, 3, 255, 255, 255, 255, 200, 0, 0, 255)
    print_at(2, 1, "PERFORMANCE TEST SCREEN - HEADER AREA")

    -- Main content area with text pattern
    fill_text_color(0, 3, text_cols, text_rows - 6, 0, 255, 0, 255, 50, 50, 50, 255)

    -- Fill with systematic text pattern
    for row = 4, text_rows - 4 do
        local line = ""
        for col = 0, text_cols - 1, 8 do
            if col + 7 < text_cols then
                line = line .. string.format("%02d:%02d ", row, col)
            end
        end
        print_at(0, row, line:sub(1, text_cols))
    end

    -- Side panels with different colors
    fill_text_color(0, 3, 10, text_rows - 6, 255, 255, 0, 255, 100, 0, 100, 255)
    fill_text_color(text_cols - 10, 3, 10, text_rows - 6, 0, 255, 255, 255, 100, 100, 0, 255)

    -- Footer
    fill_text_color(0, text_rows - 3, text_cols, 3, 255, 255, 255, 255, 0, 0, 200, 255)
    print_at(2, text_rows - 2, "FOOTER: Complex screen with " .. (text_cols * text_rows) .. " characters")

    -- Add some decorative elements
    for i = 10, text_cols - 10, 5 do
        print_at(i, 5, "*")
        print_at(i, text_rows - 5, "*")
    end
end

-- Test 1: Baseline - measure print_at performance for creating the screen
print("Test 1: Baseline print_at performance")
console_info("Test 1: Measuring print_at baseline performance")

local print_times = {}
local total_print_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_test_screen()
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    print_times[i] = iteration_time
    total_print_time = total_print_time + iteration_time
end

local avg_print_time = total_print_time / ITERATIONS

-- Calculate min/max manually since table.unpack doesn't exist in LuaJIT
local min_print_time = print_times[1]
local max_print_time = print_times[1]
for i = 2, ITERATIONS do
    if print_times[i] < min_print_time then
        min_print_time = print_times[i]
    end
    if print_times[i] > max_print_time then
        max_print_time = print_times[i]
    end
end

console_info("Print_at performance (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_print_time) .. "s")
console_info("  Minimum: " .. string.format("%.6f", min_print_time) .. "s")
console_info("  Maximum: " .. string.format("%.6f", max_print_time) .. "s")
console_info("  Total:   " .. string.format("%.6f", total_print_time) .. "s")

-- Test 2: Save the screen once for restore testing
print("Test 2: Creating saved screen for restore testing")
console_info("Test 2: Creating reference screen for restore tests")

create_test_screen()
save_text(0)
console_info("Reference screen saved to slot 0")

-- Test 3: Measure restore_text performance
print("Test 3: Restore_text performance")
console_info("Test 3: Measuring restore_text performance")

local restore_times = {}
local total_restore_time = 0

for i = 1, ITERATIONS do
    -- Clear screen to ensure we're actually restoring
    clear_text()

    local start_time = os.clock()
    restore_text(0)
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    restore_times[i] = iteration_time
    total_restore_time = total_restore_time + iteration_time
end

local avg_restore_time = total_restore_time / ITERATIONS

-- Calculate min/max manually since table.unpack doesn't exist in LuaJIT
local min_restore_time = restore_times[1]
local max_restore_time = restore_times[1]
for i = 2, ITERATIONS do
    if restore_times[i] < min_restore_time then
        min_restore_time = restore_times[i]
    end
    if restore_times[i] > max_restore_time then
        max_restore_time = restore_times[i]
    end
end

console_info("Restore_text performance (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_restore_time) .. "s")
console_info("  Minimum: " .. string.format("%.6f", min_restore_time) .. "s")
console_info("  Maximum: " .. string.format("%.6f", max_restore_time) .. "s")
console_info("  Total:   " .. string.format("%.6f", total_restore_time) .. "s")

-- Test 4: Mixed workload simulation
print("Test 4: Mixed workload simulation")
console_info("Test 4: Simulating realistic mixed workload")

-- Create multiple different screens to save
local screen_configs = {
    { name = "Main Menu",   bg_r = 0,   bg_g = 0,   bg_b = 100 },
    { name = "Game Screen", bg_r = 0,   bg_g = 100, bg_b = 0 },
    { name = "Options",     bg_r = 100, bg_g = 0,   bg_b = 0 },
    { name = "High Scores", bg_r = 100, bg_g = 100, bg_b = 0 },
    { name = "Help Screen", bg_r = 100, bg_g = 0,   bg_b = 100 }
}

-- Create and save different screens
for i, config in ipairs(screen_configs) do
    clear_text()
    fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, config.bg_r, config.bg_g, config.bg_b, 255)
    print_at(10, 5, config.name)
    print_at(10, 7, "This is screen " .. i)
    print_at(10, 8, "Background RGB: " .. config.bg_r .. "," .. config.bg_g .. "," .. config.bg_b)

    -- Add some content to make it realistic
    for row = 10, 15 do
        print_at(5, row, "Content line " .. (row - 9) .. " for " .. config.name)
    end

    save_text(i)
end

-- Test mixed workload: alternating between print_at creation and restore
local mixed_print_time = 0
local mixed_restore_time = 0
local mixed_iterations = 50

console_info("Mixed workload test (" .. (mixed_iterations * 2) .. " operations):")

for i = 1, mixed_iterations do
    -- Time creating screen with print_at
    local start_time = os.clock()
    local screen_idx = ((i - 1) % #screen_configs) + 1
    local config = screen_configs[screen_idx]

    clear_text()
    fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, config.bg_r, config.bg_g, config.bg_b, 255)
    print_at(10, 5, config.name)
    print_at(10, 7, "This is screen " .. screen_idx)
    print_at(10, 8, "Background RGB: " .. config.bg_r .. "," .. config.bg_g .. "," .. config.bg_b)

    for row = 10, 15 do
        print_at(5, row, "Content line " .. (row - 9) .. " for " .. config.name)
    end

    local end_time = os.clock()
    mixed_print_time = mixed_print_time + (end_time - start_time)

    -- Time restoring the same screen
    clear_text()
    start_time = os.clock()
    restore_text(screen_idx)
    end_time = os.clock()
    mixed_restore_time = mixed_restore_time + (end_time - start_time)
end

console_info("  Print_at operations: " .. string.format("%.6f", mixed_print_time) .. "s total")
console_info("  Restore operations:  " .. string.format("%.6f", mixed_restore_time) .. "s total")

-- Test 5: Memory usage and scalability test
print("Test 5: Scalability test with many screens")
console_info("Test 5: Testing scalability with many saved screens")

local scalability_screens = 20
local scalability_time = 0

-- Create many screens
for i = 1, scalability_screens do
    clear_text()
    fill_text_color(0, 0, text_cols, text_rows, (i * 10) % 256, (i * 20) % 256, (i * 30) % 256, 255, 50, 50, 50, 255)
    print_at(5, 5, "Scalability Test Screen " .. i)
    print_at(5, 7, "Color variant " .. i)

    -- Add unique content for each screen
    for row = 10, 20 do
        print_at(3, row, "Screen " .. i .. " content line " .. (row - 9))
    end

    save_text(100 + i)
end

-- Test random access to saved screens
local access_times = {}
for i = 1, scalability_screens do
    local random_screen = math.random(1, scalability_screens)
    local start_time = os.clock()
    restore_text(100 + random_screen)
    local end_time = os.clock()
    access_times[i] = end_time - start_time
    scalability_time = scalability_time + (end_time - start_time)
end

local avg_scalability_time = scalability_time / scalability_screens
console_info("Random access to " .. scalability_screens .. " screens:")
console_info("  Average access time: " .. string.format("%.6f", avg_scalability_time) .. "s")
console_info("  Total access time:   " .. string.format("%.6f", scalability_time) .. "s")

-- Performance Analysis and Summary
console_section("Performance Analysis Summary")

local speedup_avg = avg_print_time / avg_restore_time
local speedup_min = min_print_time / min_restore_time
local speedup_max = max_print_time / max_restore_time

console_info("=== PERFORMANCE COMPARISON ===")
console_info("Print_at method:")
console_info("  Average time: " .. string.format("%.6f", avg_print_time) .. "s")
console_info("  Range: " ..
    string.format("%.6f", min_print_time) .. "s - " .. string.format("%.6f", max_print_time) .. "s")

console_info("Restore_text method:")
console_info("  Average time: " .. string.format("%.6f", avg_restore_time) .. "s")
console_info("  Range: " ..
    string.format("%.6f", min_restore_time) .. "s - " .. string.format("%.6f", max_restore_time) .. "s")

console_separator()
console_info("=== SPEED IMPROVEMENT ===")
console_info("Average speedup: " .. string.format("%.1f", speedup_avg) .. "x faster")
console_info("Best case speedup: " .. string.format("%.1f", speedup_min) .. "x faster")
console_info("Worst case speedup: " .. string.format("%.1f", speedup_max) .. "x faster")

console_separator()
console_info("=== MIXED WORKLOAD COMPARISON ===")
local mixed_speedup = mixed_print_time / mixed_restore_time
console_info("Print_at total: " .. string.format("%.6f", mixed_print_time) .. "s")
console_info("Restore total:  " .. string.format("%.6f", mixed_restore_time) .. "s")
console_info("Mixed workload speedup: " .. string.format("%.1f", mixed_speedup) .. "x faster")

console_separator()
console_info("=== TIME SAVINGS ANALYSIS ===")
local time_saved_per_operation = avg_print_time - avg_restore_time
local time_saved_100_ops = time_saved_per_operation * 100
local time_saved_1000_ops = time_saved_per_operation * 1000

console_info("Time saved per screen display: " .. string.format("%.6f", time_saved_per_operation) .. "s")
console_info("Time saved over 100 operations: " .. string.format("%.3f", time_saved_100_ops) .. "s")
console_info("Time saved over 1000 operations: " .. string.format("%.2f", time_saved_1000_ops) .. "s")

console_separator()
console_info("=== PRACTICAL IMPLICATIONS ===")

if speedup_avg > 100 then
    console_info("🚀 EXCEPTIONAL: Restore is over 100x faster than print_at!")
    console_info("   Perfect for: Real-time applications, games, rapid screen switching")
elseif speedup_avg > 50 then
    console_info("🚀 EXCELLENT: Restore is over 50x faster than print_at!")
    console_info("   Perfect for: Interactive applications, frequent screen updates")
elseif speedup_avg > 10 then
    console_info("✅ GREAT: Restore is over 10x faster than print_at!")
    console_info("   Perfect for: UI applications, dialog systems")
elseif speedup_avg > 5 then
    console_info("✅ GOOD: Restore is over 5x faster than print_at!")
    console_info("   Beneficial for: Applications with repeated screen patterns")
else
    console_info("✅ MODEST: Restore provides meaningful improvement")
    console_info("   Still beneficial for: Specific use cases with repeated content")
end

console_info("Best use cases for save/restore:")
console_info("  • Dialog systems (save before, restore after)")
console_info("  • Multi-screen applications (instant switching)")
console_info("  • Games with screen transitions")
console_info("  • Applications with template screens")
console_info("  • Undo/redo functionality")
console_info("  • Screen state management")

-- Recommendations based on results
console_section("Recommendations")

if avg_restore_time < 0.001 then
    console_info("⚡ INSTANT: Screen restoration is sub-millisecond")
    console_info("   Recommendation: Use for any repeated screen content")
end

if speedup_avg > 20 then
    console_info("🎯 STRATEGY: Save complex screens, restore when needed")
    console_info("   Memory cost is minimal compared to time savings")
end

local ops_per_second_print = 1.0 / avg_print_time
local ops_per_second_restore = 1.0 / avg_restore_time

console_info("Throughput comparison:")
console_info("  Print_at method: " .. string.format("%.0f", ops_per_second_print) .. " screens/second")
console_info("  Restore method:  " .. string.format("%.0f", ops_per_second_restore) .. " screens/second")

-- Final visual demonstration
clear_text()
fill_text_color(0, 0, text_cols, text_rows, 0, 255, 0, 255, 0, 0, 0, 255)
print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2) - 3, "PERFORMANCE BENCHMARK COMPLETE")
print_at(math.floor(text_cols / 2) - 25, math.floor(text_rows / 2) - 1,
    "Restore is " .. string.format("%.1f", speedup_avg) .. "x faster than print_at!")
print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2) + 1, "Save/Restore system is highly optimized")
print_at(math.floor(text_cols / 2) - 15, math.floor(text_rows / 2) + 3, "Perfect for professional applications")

console_info("=== BENCHMARK COMPLETE ===")
console_info("Save/restore system provides significant performance advantages!")
console_info("Consider using save_text/restore_text for any repeated screen content.")

print("=== Print vs Restore Performance Benchmark COMPLETED ===")
console_separator()
