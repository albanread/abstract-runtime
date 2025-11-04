-- Dual Output Test
-- Tests the difference between print() and console functions
-- Shows how characters are handled in both output streams

print("=== Dual Output Test ===")
print("Testing print() vs console functions for character display")

-- Test 1: Basic output comparison
print("Test 1: Basic output comparison")
print("This message uses print() - captured by test runner")
console_info("This message uses console_info() - goes to terminal")

-- Test 2: Character display through print()
print("Test 2: Character display via print()")
print("ASCII letters: abcdefghijklmnopqrstuvwxyz")
print("Numbers: 0123456789")
print("Basic symbols: !@#$%^&*()")
print("Punctuation: .,;:?!")
print("Brackets: []{}()<>")
print("Math: +-*/=")
print("Quotes: \"'`")
print("Separators: |\\_/-~")

-- Test 3: Character display through console functions
print("Test 3: Character display via console functions")
console_section("Console Character Test")
console_info("ASCII letters: abcdefghijklmnopqrstuvwxyz")
console_info("Numbers: 0123456789")
console_info("Basic symbols: !@#$%^&*()")
console_info("Punctuation: .,;:?!")
console_info("Brackets: []{}()<>")
console_info("Math: +-*/=")
console_info("Quotes: \"'`")
console_info("Separators: |\\_/-~")

-- Test 4: Special characters that might cause issues
print("Test 4: Potentially problematic characters")
print("Backslash: \\")
print("Single backslash in string: \\\\")
print("Tab character: [TAB]")
print("Pipe symbol: |")
console_info("Console - Backslash: \\")
console_info("Console - Tab character: [TAB]")
console_info("Console - Pipe symbol: |")

-- Test 5: Mixed content with real-world examples
print("Test 5: Real-world content examples")
print("File path: /home/user/documents/test_file.lua")
print("URL: https://example.com/api/v1/data?format=json&limit=100")
print("Email: developer@runtime.com")
print("Version: v2.1.3-beta.4")
print("Command: ./run_tests --verbose --output=results.txt")

console_section("Console Real-World Examples")
console_info("File path: /home/user/documents/test_file.lua")
console_info("URL: https://example.com/api/v1/data?format=json&limit=100")
console_info("Email: developer@runtime.com")
console_info("Version: v2.1.3-beta.4")
console_info("Command: ./run_tests --verbose --output=results.txt")

-- Test 6: Long strings and formatting
print("Test 6: Long string handling")
local long_message =
"This is a very long message designed to test how both output methods handle extended text content with various characters including spaces, punctuation, numbers like 123, symbols like @#$, and other content that might cause display issues in terminal output systems."
print("Print long: " .. long_message)
console_info("Console long: " .. long_message)

-- Test 7: Rapid output comparison
print("Test 7: Rapid output test")
print("=== Rapid Print Output ===")
for i = 1, 5 do
    print("Print message #" .. i .. " with symbols: !@#$%")
end

console_section("Rapid Console Output")
for i = 1, 5 do
    console_info("Console message #" .. i .. " with symbols: !@#$%")
end

-- Test 8: Different console message types
print("Test 8: Console message type variety")
console_section("Message Type Demonstration")
console_info("This is an info message")
console_warning("This is a warning message")
console_error("This is an error message (not a real error!)")
console_debug("This is a debug message")
console_print("This is a plain console print")
console_separator()

-- Test 9: Mixed interleaved output
print("Test 9: Interleaved output test")
print("Print: Line 1")
console_info("Console: Line 2")
print("Print: Line 3 with symbols: <>{}[]")
console_info("Console: Line 4 with symbols: <>{}[]")
print("Print: Line 5")
console_info("Console: Line 6")

-- Test 10: Edge cases and escape sequences
print("Test 10: Edge cases")
print("Empty string test: '" .. "" .. "'")
print("Space only: '" .. "   " .. "'")
print("Escaped quotes: He said \"Hello World!\"")
print("Escaped backslash: C:\\\\Program Files\\\\App")
print("Mixed escapes: Path is \"C:\\\\temp\\\\file.txt\"")

console_section("Console Edge Cases")
console_info("Empty string test: '" .. "" .. "'")
console_info("Space only: '" .. "   " .. "'")
console_info("Escaped quotes: He said \"Hello World!\"")
console_info("Escaped backslash: C:\\\\Program Files\\\\App")
console_info("Mixed escapes: Path is \"C:\\\\temp\\\\file.txt\"")

-- Test 11: Summary comparison
print("Test 11: Output method summary")
console_section("Dual Output Test Summary")
print("PRINT SUMMARY: All print() messages should appear in test runner output")
console_info("CONSOLE SUMMARY: All console_*() messages should appear in terminal")
console_info("Both methods should handle all standard ASCII characters correctly")
console_info("Special characters and symbols should display properly in both")
console_separator()

print("=== Dual Output Test COMPLETED ===")
print("Check both test runner output AND terminal output for complete results")
console_section("Test Complete")
console_info("Dual output test finished successfully!")
console_info("Compare captured output vs terminal output to verify both streams")
