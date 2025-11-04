-- Character and String Handling Test
-- Tests how different characters and strings are handled by console output

print("=== Character Handling Test ===")
print("Testing character and string output handling")

-- Test 1: Basic ASCII characters
print("Test 1: Basic ASCII characters")
console_section("ASCII Character Test")
console_info("Letters: abcdefghijklmnopqrstuvwxyz")
console_info("LETTERS: ABCDEFGHIJKLMNOPQRSTUVWXYZ")
console_info("Numbers: 0123456789")

-- Test 2: Safe punctuation and symbols
print("Test 2: Safe punctuation")
console_info("Basic punctuation: . , ; : ? !")
console_info("Quotes and apostrophes: ' \" `")
console_info("Math operators: + - * / = %")
console_info("Comparison: < > =")

-- Test 3: Brackets and grouping
print("Test 3: Brackets and grouping")
console_info("Round brackets: ( )")
console_info("Square brackets: [ ]")
console_info("Curly brackets: { }")
console_info("Angle brackets: < >")

-- Test 4: Special symbols (one at a time for safety)
print("Test 4: Special symbols individually")
console_info("At symbol: @")
console_info("Hash symbol: #")
console_info("Dollar symbol: $")
console_info("Percent symbol: %")
console_info("Caret symbol: ^")
console_info("Ampersand symbol: &")
console_info("Asterisk symbol: *")
console_info("Pipe symbol: |")
console_info("Backslash symbol: \\")
console_info("Forward slash symbol: /")
console_info("Underscore symbol: _")
console_info("Dash symbol: -")
console_info("Tilde symbol: ~")

-- Test 5: String concatenation and variables
print("Test 5: String operations")
local test_string = "Hello"
local test_number = 42
console_info("String variable: " .. test_string)
console_info("Number to string: " .. tostring(test_number))
console_info("Mixed: " .. test_string .. " " .. tostring(test_number))

-- Test 6: Escape sequences in strings
print("Test 6: Escape sequences")
console_info("Tab test: before\\ttab\\tafter")
console_info("Newline test: before\\nafter")
console_info("Quote test: He said \\\"Hello\\\"")
console_info("Backslash test: C:\\\\path\\\\to\\\\file")

-- Test 7: Long strings
print("Test 7: Long string handling")
local long_string =
"This is a very long string that contains many words and should test how the console handles extended character sequences without any special characters that might cause issues."
console_info("Long string: " .. long_string)

-- Test 8: Empty and whitespace strings
print("Test 8: Edge case strings")
console_info("Empty string: '" .. "" .. "'")
console_info("Space string: '" .. " " .. "'")
console_info("Multiple spaces: '" .. "   " .. "'")
console_info("Tab character: '" .. "\t" .. "'")

-- Test 9: Repeated characters
print("Test 9: Repeated characters")
console_info("Repeated A: " .. string.rep("A", 10))
console_info("Repeated dash: " .. string.rep("-", 20))
console_info("Repeated dot: " .. string.rep(".", 15))

-- Test 10: Mixed content
print("Test 10: Mixed content test")
console_section("Mixed Content Example")
console_info("File path example: /home/user/documents/file.txt")
console_info("URL example: https://example.com/path?param=value")
console_info("Email example: user@domain.com")
console_info("Version example: v1.2.3-beta")
console_separator()

-- Test 11: Unicode-safe characters
print("Test 11: Unicode-safe characters")
console_info("Copyright: (C) 2024")
console_info("Trademark: MyApp (TM)")
console_info("Registered: MyBrand (R)")
console_info("Degrees: 45 degrees")

-- Test 12: Programming-related strings
print("Test 12: Programming strings")
console_info("Function call: console_info(message)")
console_info("Variable assignment: local x = 42")
console_info("Array access: array[index]")
console_info("Object property: object.property")

print("=== Character Handling Test COMPLETED ===")
console_section("Test Summary")
console_info("All character types tested")
console_info("Check console output for any missing characters")
console_separator()
