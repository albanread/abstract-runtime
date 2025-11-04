-- Print At Performance Test
-- Measures how long it takes to fill the entire text display using print_at
-- This test provides benchmarks for text rendering performance

print("=== Print At Performance Test ===")
print("Measuring text display filling performance")

-- Test setup
console_section("Print At Performance Benchmark")
console_info("Starting text rendering performance measurement")

-- Get screen dimensions first
local screen_width = get_screen_width()
local screen_height = get_screen_height()
console_info("Screen dimensions: " .. screen_width .. "x" .. screen_height)

-- Calculate text grid dimensions (assuming standard character size)
-- Most runtimes use approximately 8x16 pixel characters
local char_width = 8
local char_height = 16
local text_cols = math.floor(screen_width / char_width)
local text_rows = math.floor(screen_height / char_height)

console_info("Estimated text grid: " .. text_cols .. " columns x " .. text_rows .. " rows")
console_info("Total characters to fill: " .. (text_cols * text_rows))

-- Test 1: Fill with single character
print("Test 1: Fill entire display with single character")
console_info("Test 1: Single character fill performance")

clear_text()
set_text_ink(255, 255, 255, 255) -- White text

local start_time = os.clock()

for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        print_at(col, row, "X")
    end
end

local single_char_time = os.clock() - start_time
console_info("Single character fill time: " .. string.format("%.3f", single_char_time) .. " seconds")
console_info("Characters per second: " .. string.format("%.0f", (text_cols * text_rows) / single_char_time))

-- Wait a moment to see the result
print("Pausing to display results...")
if os.execute then
    os.execute("sleep 1") -- Unix/Linux/macOS
else
    -- Fallback busy wait
    local pause_start = os.clock()
    while os.clock() - pause_start < 1 do
        -- busy wait
    end
end

-- Test 2: Fill with different characters
print("Test 2: Fill with varying characters")
console_info("Test 2: Varying character fill performance")

clear_text()
set_text_ink(100, 255, 100, 255) -- Green text

start_time = os.clock()

for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        local char_code = (row * text_cols + col) % 26
        local char = string.char(65 + char_code) -- A-Z cycling
        print_at(col, row, char)
    end
end

local varying_char_time = os.clock() - start_time
console_info("Varying character fill time: " .. string.format("%.3f", varying_char_time) .. " seconds")
console_info("Characters per second: " .. string.format("%.0f", (text_cols * text_rows) / varying_char_time))

-- Pause again
if os.execute then
    os.execute("sleep 1")
else
    local pause_start = os.clock()
    while os.clock() - pause_start < 1 do end
end

-- Test 3: Fill with numbers
print("Test 3: Fill with numbers")
console_info("Test 3: Number character fill performance")

clear_text()
set_text_ink(100, 100, 255, 255) -- Blue text

start_time = os.clock()

for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        local digit = (row * text_cols + col) % 10
        print_at(col, row, tostring(digit))
    end
end

local number_char_time = os.clock() - start_time
console_info("Number character fill time: " .. string.format("%.3f", number_char_time) .. " seconds")
console_info("Characters per second: " .. string.format("%.0f", (text_cols * text_rows) / number_char_time))

-- Pause
if os.execute then
    os.execute("sleep 1")
else
    local pause_start = os.clock()
    while os.clock() - pause_start < 1 do end
end

-- Test 4: Fill with special characters
print("Test 4: Fill with special characters")
console_info("Test 4: Special character fill performance")

clear_text()
set_text_ink(255, 255, 100, 255) -- Yellow text

local special_chars = { "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "-", "+", "=", "[", "]", "{", "}", "|", "\\",
    ":", ";", "'", "\"", "<", ">", ",", ".", "?", "/" }

start_time = os.clock()

for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        local char_index = ((row * text_cols + col) % #special_chars) + 1
        print_at(col, row, special_chars[char_index])
    end
end

local special_char_time = os.clock() - start_time
console_info("Special character fill time: " .. string.format("%.3f", special_char_time) .. " seconds")
console_info("Characters per second: " .. string.format("%.0f", (text_cols * text_rows) / special_char_time))

-- Pause
if os.execute then
    os.execute("sleep 1")
else
    local pause_start = os.clock()
    while os.clock() - pause_start < 1 do end
end

-- Test 5: Fill with color changes
print("Test 5: Fill with color changes")
console_info("Test 5: Color-changing fill performance")

clear_text()

start_time = os.clock()

for row = 0, text_rows - 1 do
    for col = 0, text_cols - 1 do
        -- Change color based on position
        local red = (col * 255) / text_cols
        local green = (row * 255) / text_rows
        local blue = ((col + row) * 128) / (text_cols + text_rows)

        set_text_ink(red, green, blue, 255)
        print_at(col, row, "*")
    end
end

local color_change_time = os.clock() - start_time
console_info("Color-changing fill time: " .. string.format("%.3f", color_change_time) .. " seconds")
console_info("Characters per second: " .. string.format("%.0f", (text_cols * text_rows) / color_change_time))

-- Test 6: Performance comparison summary
print("Test 6: Performance analysis")
console_section("Performance Test Results Summary")

local total_chars = text_cols * text_rows

console_info("Screen: " .. screen_width .. "x" .. screen_height .. " pixels")
console_info("Text grid: " .. text_cols .. "x" .. text_rows .. " characters")
console_info("Total characters: " .. total_chars)
console_separator()

console_info("Single character:   " ..
string.format("%6.3f", single_char_time) ..
"s (" .. string.format("%6.0f", total_chars / single_char_time) .. " chars/sec)")
console_info("Varying characters: " ..
string.format("%6.3f", varying_char_time) ..
"s (" .. string.format("%6.0f", total_chars / varying_char_time) .. " chars/sec)")
console_info("Number characters:  " ..
string.format("%6.3f", number_char_time) ..
"s (" .. string.format("%6.0f", total_chars / number_char_time) .. " chars/sec)")
console_info("Special characters: " ..
string.format("%6.3f", special_char_time) ..
"s (" .. string.format("%6.0f", total_chars / special_char_time) .. " chars/sec)")
console_info("With color changes: " ..
string.format("%6.3f", color_change_time) ..
"s (" .. string.format("%6.0f", total_chars / color_change_time) .. " chars/sec)")

console_separator()

-- Find fastest and slowest
local times = {
    { name = "Single character",   time = single_char_time },
    { name = "Varying characters", time = varying_char_time },
    { name = "Number characters",  time = number_char_time },
    { name = "Special characters", time = special_char_time },
    { name = "With color changes", time = color_change_time }
}

table.sort(times, function(a, b) return a.time < b.time end)

console_info("Fastest: " .. times[1].name .. " (" .. string.format("%.3f", times[1].time) .. "s)")
console_info("Slowest: " .. times[#times].name .. " (" .. string.format("%.3f", times[#times].time) .. "s)")

local speed_difference = times[#times].time / times[1].time
console_info("Speed difference: " .. string.format("%.1f", speed_difference) .. "x")

-- Test 7: Partial screen performance
print("Test 7: Partial screen fills")
console_section("Partial Screen Fill Tests")

-- Quarter screen
local quarter_cols = math.floor(text_cols / 2)
local quarter_rows = math.floor(text_rows / 2)

clear_text()
set_text_ink(255, 100, 100, 255) -- Red

start_time = os.clock()
for row = 0, quarter_rows - 1 do
    for col = 0, quarter_cols - 1 do
        print_at(col, row, "Q")
    end
end
local quarter_time = os.clock() - start_time

console_info("Quarter screen (" .. (quarter_cols * quarter_rows) .. " chars): " ..
    string.format("%.3f", quarter_time) .. "s (" ..
    string.format("%.0f", (quarter_cols * quarter_rows) / quarter_time) .. " chars/sec)")

-- Single row
clear_text()
set_text_ink(100, 255, 100, 255) -- Green

start_time = os.clock()
for col = 0, text_cols - 1 do
    print_at(col, 0, "R")
end
local row_time = os.clock() - start_time

console_info("Single row (" .. text_cols .. " chars): " ..
    string.format("%.6f", row_time) .. "s (" ..
    string.format("%.0f", text_cols / row_time) .. " chars/sec)")

-- Single column
clear_text()
set_text_ink(100, 100, 255, 255) -- Blue

start_time = os.clock()
for row = 0, text_rows - 1 do
    print_at(0, row, "C")
end
local col_time = os.clock() - start_time

console_info("Single column (" .. text_rows .. " chars): " ..
    string.format("%.6f", col_time) .. "s (" ..
    string.format("%.0f", text_rows / col_time) .. " chars/sec)")

-- Test conclusions
console_section("Performance Test Conclusions")
console_info("print_at performance varies based on:")
console_info("1. Character content (consistent vs varying)")
console_info("2. Color changes (major performance impact)")
console_info("3. Screen position (row vs column access patterns)")
console_info("4. Total number of characters")

local avg_chars_per_sec = total_chars /
((single_char_time + varying_char_time + number_char_time + special_char_time) / 4)
console_info("Average performance: " .. string.format("%.0f", avg_chars_per_sec) .. " characters/second")

if avg_chars_per_sec > 10000 then
    console_info("✅ Excellent text rendering performance")
elseif avg_chars_per_sec > 5000 then
    console_info("✅ Good text rendering performance")
elseif avg_chars_per_sec > 1000 then
    console_info("⚠️  Moderate text rendering performance")
else
    console_info("❌ Slow text rendering performance - may need optimization")
end

-- Final display
clear_text()
set_text_ink(255, 255, 255, 255)
print_at(0, 0, "PERFORMANCE TEST COMPLETE")
print_at(0, 1, "Check console for detailed results")
print_at(0, 2, "Average: " .. string.format("%.0f", avg_chars_per_sec) .. " chars/sec")

print("=== Print At Performance Test COMPLETED ===")
console_info("Performance benchmark completed successfully!")
console_separator()
