-- Actual Render Timing Test
-- Uses wait_for_render_complete() to measure real render completion time
-- This eliminates command queuing time and measures actual GPU rendering performance

print("=== Actual Render Timing Test ===")
print("Measuring true render completion time vs command queuing time")

console_section("Actual Render Performance Analysis")
console_info("Using wait_for_render_complete() to measure real rendering time")

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
local ITERATIONS = 50 -- Fewer iterations since we're waiting for actual rendering

-- Helper function to create test content
local function create_complex_screen()
    clear_text()

    -- Header
    fill_text_color(0, 0, text_cols, 3, 255, 255, 255, 255, 200, 0, 0, 255)
    print_at(2, 1, "RENDER TIMING TEST - COMPLEX SCREEN")

    -- Body with pattern
    fill_text_color(0, 3, text_cols, text_rows - 6, 0, 255, 0, 255, 50, 50, 50, 255)

    -- Fill with text pattern
    for row = 4, text_rows - 4 do
        local line = ""
        for col = 0, text_cols - 1, 10 do
            if col + 9 < text_cols then
                line = line .. string.format("R%02dC%02d ", row, col)
            end
        end
        if #line > 0 then
            print_at(0, row, line:sub(1, text_cols))
        end
    end

    -- Colorful side panels
    fill_text_color(0, 3, 8, text_rows - 6, 255, 255, 0, 255, 100, 0, 100, 255)
    fill_text_color(text_cols - 8, 3, 8, text_rows - 6, 0, 255, 255, 255, 100, 100, 0, 255)

    -- Footer
    fill_text_color(0, text_rows - 3, text_cols, 3, 255, 255, 255, 255, 0, 0, 200, 255)
    print_at(2, text_rows - 2, "Footer: " .. (text_cols * text_rows) .. " characters total")
end

-- Test 1: Command queuing time (baseline)
print("Test 1: Command queuing time measurement")
console_info("Test 1: Measuring command queuing time (no render wait)")

local command_times = {}
local total_command_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_complex_screen()
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    command_times[i] = iteration_time
    total_command_time = total_command_time + iteration_time
end

local avg_command_time = total_command_time / ITERATIONS

-- Calculate min/max manually
local min_command_time = command_times[1]
local max_command_time = command_times[1]
for i = 2, ITERATIONS do
    if command_times[i] < min_command_time then
        min_command_time = command_times[i]
    end
    if command_times[i] > max_command_time then
        max_command_time = command_times[i]
    end
end

console_info("Command queuing performance (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_command_time) .. "s")
console_info("  Minimum: " .. string.format("%.6f", min_command_time) .. "s")
console_info("  Maximum: " .. string.format("%.6f", max_command_time) .. "s")
console_info("  Total:   " .. string.format("%.3f", total_command_time) .. "s")

-- Test 2: Actual render completion time
print("Test 2: Actual render completion time")
console_info("Test 2: Measuring actual render completion time")

local render_times = {}
local total_render_time = 0

for i = 1, ITERATIONS do
    local start_time = os.clock()
    create_complex_screen()
    wait_for_render_complete() -- Wait for actual rendering to complete
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    render_times[i] = iteration_time
    total_render_time = total_render_time + iteration_time
end

local avg_render_time = total_render_time / ITERATIONS

-- Calculate min/max manually
local min_render_time = render_times[1]
local max_render_time = render_times[1]
for i = 2, ITERATIONS do
    if render_times[i] < min_render_time then
        min_render_time = render_times[i]
    end
    if render_times[i] > max_render_time then
        max_render_time = render_times[i]
    end
end

console_info("Actual render performance (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_render_time) .. "s")
console_info("  Minimum: " .. string.format("%.6f", min_render_time) .. "s")
console_info("  Maximum: " .. string.format("%.6f", max_render_time) .. "s")
console_info("  Total:   " .. string.format("%.3f", total_render_time) .. "s")

-- Test 3: Save/restore with command queuing
print("Test 3: Save/restore command queuing time")
console_info("Test 3: Save/restore command queuing performance")

-- Create and save reference screen
create_complex_screen()
save_text(0)

local restore_command_times = {}
local total_restore_command_time = 0

for i = 1, ITERATIONS do
    clear_text() -- Clear first

    local start_time = os.clock()
    restore_text(0)
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    restore_command_times[i] = iteration_time
    total_restore_command_time = total_restore_command_time + iteration_time
end

local avg_restore_command_time = total_restore_command_time / ITERATIONS

console_info("Restore command queuing (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_restore_command_time) .. "s")
console_info("  Total:   " .. string.format("%.6f", total_restore_command_time) .. "s")

-- Test 4: Save/restore with actual render completion
print("Test 4: Save/restore actual render time")
console_info("Test 4: Save/restore actual render completion")

local restore_render_times = {}
local total_restore_render_time = 0

for i = 1, ITERATIONS do
    clear_text() -- Clear first

    local start_time = os.clock()
    restore_text(0)
    wait_for_render_complete() -- Wait for actual rendering
    local end_time = os.clock()

    local iteration_time = end_time - start_time
    restore_render_times[i] = iteration_time
    total_restore_render_time = total_restore_render_time + iteration_time
end

local avg_restore_render_time = total_restore_render_time / ITERATIONS

console_info("Restore actual render (" .. ITERATIONS .. " iterations):")
console_info("  Average: " .. string.format("%.6f", avg_restore_render_time) .. "s")
console_info("  Total:   " .. string.format("%.3f", total_restore_render_time) .. "s")

-- Test 5: Frame-by-frame timing analysis
print("Test 5: Frame timing analysis")
console_info("Test 5: Individual frame render timing")

local frame_times = {}
local total_frame_time = 0

for i = 1, 20 do -- Smaller sample for frame analysis
    -- Create different content each frame
    clear_text()
    fill_text_color(0, 0, text_cols, text_rows,
        (i * 20) % 256, (i * 30) % 256, (i * 40) % 256, 255,
        50, 50, 50, 255)
    print_at(10, 10, "Frame " .. i .. " timing test")
    print_at(10, 12, "Measuring individual frame render time")

    local start_time = os.clock()
    wait_for_render_complete()
    local end_time = os.clock()

    local frame_time = end_time - start_time
    frame_times[i] = frame_time
    total_frame_time = total_frame_time + frame_time

    console_info("Frame " .. i .. ": " .. string.format("%.6f", frame_time) .. "s")
end

local avg_frame_time = total_frame_time / 20
console_info("Frame timing analysis:")
console_info("  Average frame time: " .. string.format("%.6f", avg_frame_time) .. "s")
console_info("  Estimated FPS: " .. string.format("%.1f", 1.0 / avg_frame_time))

-- Performance Analysis
console_section("Performance Analysis")

local render_overhead = avg_render_time - avg_command_time
local restore_overhead = avg_restore_render_time - avg_restore_command_time

console_info("=== COMMAND VS RENDER COMPARISON ===")
console_info("Print_at operations:")
console_info("  Command queuing: " .. string.format("%.6f", avg_command_time) .. "s")
console_info("  Actual rendering: " .. string.format("%.6f", avg_render_time) .. "s")
console_info("  Render overhead: " .. string.format("%.6f", render_overhead) .. "s")

if render_overhead > 0 then
    local overhead_factor = avg_render_time / avg_command_time
    console_info("  Overhead factor: " .. string.format("%.1f", overhead_factor) .. "x")
end

console_info("Restore operations:")
console_info("  Command queuing: " .. string.format("%.6f", avg_restore_command_time) .. "s")
console_info("  Actual rendering: " .. string.format("%.6f", avg_restore_render_time) .. "s")
console_info("  Render overhead: " .. string.format("%.6f", restore_overhead) .. "s")

if restore_overhead > 0 then
    local restore_overhead_factor = avg_restore_render_time / avg_restore_command_time
    console_info("  Overhead factor: " .. string.format("%.1f", restore_overhead_factor) .. "x")
end

console_separator()
console_info("=== TRUE PERFORMANCE COMPARISON ===")
local true_speedup = avg_render_time / avg_restore_render_time
console_info("Print_at (actual): " .. string.format("%.6f", avg_render_time) .. "s")
console_info("Restore (actual):  " .. string.format("%.6f", avg_restore_render_time) .. "s")
console_info("True speedup: " .. string.format("%.1f", true_speedup) .. "x faster")

console_separator()
console_info("=== THROUGHPUT ANALYSIS ===")
local print_throughput = 1.0 / avg_render_time
local restore_throughput = 1.0 / avg_restore_render_time

console_info("Print_at throughput: " .. string.format("%.1f", print_throughput) .. " screens/second")
console_info("Restore throughput:  " .. string.format("%.1f", restore_throughput) .. " screens/second")

console_separator()
console_info("=== FRAME RATE IMPACT ===")
local target_fps = 60
local frame_budget = 1.0 / target_fps

console_info("60 FPS frame budget: " .. string.format("%.6f", frame_budget) .. "s")
console_info("Print_at frame cost: " .. string.format("%.1f", (avg_render_time / frame_budget) * 100) .. "% of frame")
console_info("Restore frame cost:  " ..
string.format("%.1f", (avg_restore_render_time / frame_budget) * 100) .. "% of frame")

if avg_render_time > frame_budget then
    console_info("⚠️  WARNING: Print_at operations exceed 60 FPS budget")
elseif avg_render_time > frame_budget * 0.5 then
    console_info("⚠️  CAUTION: Print_at operations use significant frame time")
else
    console_info("✅ GOOD: Print_at operations fit within frame budget")
end

if avg_restore_render_time > frame_budget then
    console_info("⚠️  WARNING: Restore operations exceed 60 FPS budget")
elseif avg_restore_render_time > frame_budget * 0.1 then
    console_info("⚠️  CAUTION: Restore operations use noticeable frame time")
else
    console_info("✅ EXCELLENT: Restore operations use minimal frame time")
end

-- Recommendations
console_section("Recommendations")

if render_overhead > avg_command_time then
    console_info("📊 FINDING: Render overhead is significant")
    console_info("   Command queuing is much faster than actual rendering")
    console_info("   Previous benchmarks may have been misleading")
else
    console_info("📊 FINDING: Render overhead is minimal")
    console_info("   Command queuing time approximates actual render time")
end

if true_speedup > 5 then
    console_info("🚀 RECOMMENDATION: Save/restore still provides significant benefit")
    console_info("   " .. string.format("%.1f", true_speedup) .. "x speedup justifies memory usage")
elseif true_speedup > 2 then
    console_info("✅ RECOMMENDATION: Save/restore provides meaningful benefit")
    console_info("   " .. string.format("%.1f", true_speedup) .. "x speedup worth considering")
else
    console_info("⚖️  RECOMMENDATION: Save/restore provides modest benefit")
    console_info("   Consider based on specific use case requirements")
end

console_info("💡 KEY INSIGHT: Use wait_for_render_complete() for accurate benchmarks")
console_info("   This eliminates misleading command queuing measurements")

-- Final display
clear_text()
fill_text_color(0, 0, text_cols, text_rows, 255, 255, 255, 255, 0, 100, 0, 255)
print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2) - 2, "ACTUAL RENDER TIMING TEST COMPLETE")
print_at(math.floor(text_cols / 2) - 25, math.floor(text_rows / 2),
    "True speedup: " .. string.format("%.1f", true_speedup) .. "x (not just command queuing)")
print_at(math.floor(text_cols / 2) - 20, math.floor(text_rows / 2) + 2, "Accurate measurements with render sync")

console_info("=== TEST COMPLETE ===")
console_info("Actual render timing measured with wait_for_render_complete()")
console_info("This provides true performance data, not just command queuing speed")

print("=== Actual Render Timing Test COMPLETED ===")
console_separator()
