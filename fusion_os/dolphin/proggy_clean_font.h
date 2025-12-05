/*
 * proggy_clean_font.h - proggy clean bitmap font implementation
 * 
 * implements proggy clean tt font as bitmap data for terminal display
 * with proper character rendering and text output
 */

#ifndef PROGGY_CLEAN_FONT_H
#define PROGGY_CLEAN_FONT_H

#include <stdint.h>
#include <stddef.h>

/* font configuration - proggy clean tt characteristics */
#define PROGGY_FONT_WIDTH    8
#define PROGGY_FONT_HEIGHT   13
#define PROGGY_FONT_BASELINE 11
#define PROGGY_FONT_CHARS    256
#define PROGGY_FONT_SIZE     (PROGGY_FONT_WIDTH * PROGGY_FONT_HEIGHT * PROGGY_FONT_CHARS)

/* font bitmap structure */
typedef struct {
    uint8_t glyph_width;
    uint8_t glyph_height;
    uint8_t first_char;
    uint8_t char_count;
    uint8_t data[PROGGY_FONT_SIZE];
} proggy_font_t;

/* font character information */
typedef struct {
    uint8_t width;
    uint8_t height;
    uint32_t bitmap_offset;
} font_char_info_t;

/* terminal text colors */
#define COLOR_BLACK     0x00000000
#define COLOR_RED       0x00ff0000
#define COLOR_GREEN     0x0000ff00
#define COLOR_BLUE      0x000000ff
#define COLOR_CYAN      0x0000ffff
#define COLOR_MAGENTA   0x00ff00ff
#define COLOR_YELLOW    0x00ffff00
#define COLOR_WHITE     0x00ffffff
#define COLOR_LIGHT_GRAY    0x00808080
#define COLOR_DARK_GRAY     0x00404040
#define COLOR_BRIGHT_RED    0x00000080
#define COLOR_BRIGHT_GREEN  0x00008000
#define COLOR_BRIGHT_BLUE   0x00000080
#define COLOR_BRIGHT_CYAN   0x00008080
#define COLOR_BRIGHT_MAGENTA 0x00800080
#define COLOR_BRIGHT_YELLOW 0x00808080
#define COLOR_BRIGHT_WHITE  0x00c0c0c0

/* text attributes */
#define TEXT_NORMAL      0x00
#define TEXT_BOLD        0x01
#define TEXT_UNDERLINE   0x02
#define TEXT_INVERSE     0x04

/* font initialization */
int proggy_font_init(void);
const proggy_font_t* proggy_font_get(void);

/* character rendering */
void proggy_font_draw_char(void* framebuffer, int x, int y, uint8_t character, 
                          uint32_t foreground_color, uint32_t background_color, 
                          uint8_t attributes);

/* text rendering */
void proggy_font_draw_text(void* framebuffer, int x, int y, const char* text, 
                          uint32_t foreground_color, uint32_t background_color, 
                          uint8_t attributes);
                          
/* text measurement */
int proggy_font_measure_text(const char* text);
int proggy_font_measure_char(uint8_t character);
font_char_info_t* proggy_font_get_char_info(uint8_t character);

/* font data access */
const uint8_t* proggy_font_get_glyph_data(uint8_t character, int* glyph_width, int* glyph_height);

/* terminal text layout */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    uint32_t foreground_color;
    uint32_t background_color;
    uint8_t attributes;
} terminal_text_area_t;

void terminal_text_clear_area(terminal_text_area_t* area);
void terminal_text_draw_line(terminal_text_area_t* area, int line, const char* text);
void terminal_text_draw_char(terminal_text_area_t* area, int x, int y, uint8_t character);

#endif /* PROGGY_CLEAN_FONT_H */