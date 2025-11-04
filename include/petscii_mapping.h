/**
 * @file petscii_mapping.h
 * @brief PETSCII character mapping functions for the abstract runtime
 * 
 * This header provides functions to convert PETSCII screen codes and character codes
 * to the appropriate Unicode codepoints for rendering with the PetMe font.
 */

#ifndef PETSCII_MAPPING_H
#define PETSCII_MAPPING_H

#include <cstdint>



/**
 * Convert a PETSCII screen code to Unicode codepoint
 * @param screen_code PETSCII screen code (0-255)
 * @return Unicode codepoint for rendering
 */
uint32_t petscii_screen_to_unicode(uint8_t screen_code);

/**
 * Convert a PETSCII character code to Unicode codepoint
 * @param petscii_code PETSCII code (0-255)
 * @return Unicode codepoint for rendering
 */
uint32_t petscii_char_to_unicode(uint8_t petscii_code);

/**
 * Check if a character should be treated as a PETSCII code
 * @param ch Character to check
 * @return true if this should use PETSCII mapping
 */
bool is_petscii_graphics_char(uint8_t ch);

/**
 * Get the best Unicode codepoint for a character
 * @param ch Input character (could be ASCII or PETSCII)
 * @return Unicode codepoint to use for font rendering
 */
uint32_t get_display_codepoint(uint8_t ch);



#endif // PETSCII_MAPPING_H