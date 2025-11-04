/**
 * @file petscii_mapping.cpp
 * @brief PETSCII character mapping functions for the abstract runtime
 * 
 * This file provides functions to convert PETSCII screen codes and character codes
 * to the appropriate Unicode codepoints for rendering with the PetMe font.
 */

#include <cstdint>

// PETSCII screen code to Unicode mapping table
// Based on the PETSCII character map documentation
static const uint32_t petscii_screen_to_unicode_table[256] = {
    // $00-$0F
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    
    // $10-$1F  
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x2191, 0x2190,
    
    // $20-$2F (ASCII printable)
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    
    // $30-$3F (ASCII digits and symbols)
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    
    // $40-$4F (Box drawing and special characters)
    0x2500, 0x2660, 0x1FB72, 0x1FB78, 0x1FB77, 0x1FB76, 0x1FB7A, 0x1FB71,
    0x1FB74, 0x256E, 0x2570, 0x256F, 0x1FB7C, 0x2572, 0x2571, 0x1FB7D,
    
    // $50-$5F
    0x1FB7E, 0x2022, 0x1FB7B, 0x2665, 0x1FB70, 0x256D, 0x2573, 0x25CB,
    0x2663, 0x1FB75, 0x2666, 0x253C, 0x1FB8C, 0x2502, 0x03C0, 0x25E5,
    
    // $60-$6F (PETSCII graphics blocks)
    0x00A0, 0x258C, 0x2584, 0x2594, 0x2581, 0x258F, 0x2592, 0x2595,
    0x1FB8F, 0x25E4, 0x1FB87, 0x251C, 0x2597, 0x2514, 0x2510, 0x2582,
    
    // $70-$7F (More box drawing)
    0x250C, 0x2534, 0x252C, 0x2524, 0x258E, 0x258D, 0x1FB88, 0x1FB82,
    0x1FB83, 0x2583, 0x1FB7F, 0x2596, 0x259D, 0x2518, 0x2598, 0x259A,
    
    // $80-$FF (Extended characters, mostly ASCII for simplicity)
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087,
    0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097,
    0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

/**
 * Convert a PETSCII screen code to Unicode codepoint
 * @param screen_code PETSCII screen code (0-255)
 * @return Unicode codepoint for rendering
 */
uint32_t petscii_screen_to_unicode(uint8_t screen_code) {
    return petscii_screen_to_unicode_table[screen_code];
}

/**
 * Convert a PETSCII character code to Unicode codepoint
 * @param petscii_code PETSCII code (0-255)
 * @return Unicode codepoint for rendering
 */
uint32_t petscii_char_to_unicode(uint8_t petscii_code) {
    // For most PETSCII codes, we can map directly to screen codes
    // This is a simplified mapping - the full mapping would need
    // to handle the shifted/unshifted variations properly
    
    if (petscii_code >= 0x20 && petscii_code <= 0x3F) {
        // ASCII printable characters map directly
        return petscii_code;
    } else if (petscii_code >= 0x40 && petscii_code <= 0x5F) {
        // Letters and symbols - map to screen codes
        return petscii_screen_to_unicode_table[petscii_code - 0x40];
    } else if (petscii_code >= 0xA0 && petscii_code <= 0xBF) {
        // Graphics characters - map to screen codes 0x60-0x7F
        return petscii_screen_to_unicode_table[petscii_code - 0x40];
    } else if (petscii_code >= 0xC0 && petscii_code <= 0xDF) {
        // More graphics - map to screen codes 0x40-0x5F
        return petscii_screen_to_unicode_table[petscii_code - 0x80];
    } else if (petscii_code >= 0xE0 && petscii_code <= 0xFF) {
        // Final graphics range - map to screen codes 0x60-0x7F
        return petscii_screen_to_unicode_table[petscii_code - 0x80];
    }
    
    // Default to direct mapping for other codes
    return petscii_code;
}

/**
 * Check if a character should be treated as a PETSCII code
 * @param ch Character to check
 * @return true if this should use PETSCII mapping
 */
bool is_petscii_graphics_char(uint8_t ch) {
    // PETSCII graphics characters are typically in these ranges:
    // 0x60-0x7F (screen codes for graphics)
    // 0x80-0xFF (PETSCII codes for graphics)
    return (ch >= 0x60 && ch <= 0x7F) || (ch >= 0x80);
}

/**
 * Get the best Unicode codepoint for a character
 * @param ch Input character (could be ASCII or PETSCII)
 * @return Unicode codepoint to use for font rendering
 */
uint32_t get_display_codepoint(uint8_t ch) {
    if (ch < 0x20) {
        // Control characters - typically not displayed
        return 0x0020; // Space
    } else if (ch < 0x80) {
        // ASCII range - check if it's a PETSCII graphics character
        if (is_petscii_graphics_char(ch)) {
            return petscii_screen_to_unicode_table[ch];
        } else {
            return ch; // Direct ASCII mapping
        }
    } else {
        // Extended range - treat as PETSCII
        return petscii_char_to_unicode(ch);
    }
}