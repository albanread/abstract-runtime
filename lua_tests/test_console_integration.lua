-- Console Integration Test
-- Tests the console output functions and their integration with Lua

print("=== Console Integration Test ===")
print("Testing runtime console functions from Lua")

-- Test 1: Basic console functions should be available
print("Test 1: Check console function availability")
assert_not_nil(console_print, "console_print should be available")
assert_not_nil(console_info, "console_info should be available")
assert_not_nil(console_warning, "console_warning should be available")
assert_not_nil(console_error, "console_error should be available")
assert_not_nil(console_debug, "console_debug should be available")
assert_not_nil(console_section, "console_section should be available")
assert_not_nil(console_separator, "console_separator should be available")

-- Test 2: Console output functions
print("Test 2: Console output functions")
console_section("Lua Console Test Section")
console_info("This is an info message from Lua")
console_warning("This is a warning message from Lua")
console_debug("This is a debug message from Lua")
console_separator()

-- Test 3: Console and print integration
print("Test 3: Mixed output test")
print("This goes to test output")
console_print("This goes to console via console_print")
console_info("Console and print should work together")

-- Test 4: Test console formatting
print("Test 4: Console formatting test")
console_section("Formatting Test")
console_info("Testing various message types:")
console_print("  - Regular console print")
console_info("  - Info level message")
console_warning("  - Warning level message")
console_debug("  - Debug level message")
console_separator()

-- Test 5: Long message test
print("Test 5: Long message handling")
local long_message =
"This is a very long message that tests how the console handles extended text output from Lua scripts during testing procedures."
console_info(long_message)

-- Test 6: Multiple consecutive messages
print("Test 6: Rapid console output")
console_section("Rapid Output Test")
for i = 1, 5 do
    console_info("Rapid message #" .. i)
end
console_separator()

-- Test 7: Error level test (should still pass the test)
print("Test 7: Error level output (non-fatal)")
console_error("This is a test error message - not a real error!")
console_info("Error messages should not fail the test")

-- Test 8: Safe special characters (avoiding control chars)
print("Test 8: Special character handling")
console_info("Testing basic symbols: !@#$%^&*()")
console_info("Testing numbers: 1234567890")
console_info("Testing punctuation: .,;:?!")
console_info("Testing brackets: []{}()")
console_info("Testing quotes: \"'`")
console_info("Testing math symbols: +-*/=")
console_info("Testing comparison: <>=")
console_info("Testing separators: |\\/_-")

-- Test 9: Nested sections
print("Test 9: Nested section structure")
console_section("Main Section")
console_info("This is in the main section")
console_section("Sub Section")
console_info("This is in a sub section")
console_separator()
console_info("Back to regular output")

-- Test 10: Runtime state reporting
print("Test 10: Runtime state via console")
console_section("Runtime Status Report")
console_info("Screen dimensions: " .. get_screen_width() .. "x" .. get_screen_height())
console_info("Runtime should_quit status: " .. tostring(should_quit()))
console_separator()

print("=== Console Integration Test COMPLETED ===")
print("All console functions tested successfully!")
console_section("Test Summary")
console_info("✅ Console integration test passed!")
console_info("All console output functions are working correctly")
console_separator()
