/*
 * terminal.h - linux-like terminal implementation for fusion os
 * 
 * implements terminal emulation using framebuffer and proggy clean font
 * with proper terminal commands and interface
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>
#include "proggy_clean_font.h"

/* terminal configuration */
#define TERMINAL_WIDTH_CHARS  80
#define TERMINAL_HEIGHT_CHARS 30
#define TERMINAL_BUFFER_LINES 100

/* terminal colors (8-bit indexed) */
#define TERMINAL_COLOR_BLACK     0
#define TERMINAL_COLOR_RED       1
#define TERMINAL_COLOR_GREEN     2
#define TERMINAL_COLOR_YELLOW    3
#define TERMINAL_COLOR_BLUE      4
#define TERMINAL_COLOR_MAGENTA   5
#define TERMINAL_COLOR_CYAN      6
#define TERMINAL_COLOR_WHITE     7
#define TERMINAL_COLOR_LIGHT_GRAY    8
#define TERMINAL_COLOR_DARK_GRAY     9
#define TERMINAL_COLOR_BRIGHT_RED    10
#define TERMINAL_COLOR_BRIGHT_GREEN  11
#define TERMINAL_COLOR_BRIGHT_BLUE   12

/* terminal state */
typedef struct {
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t scroll_offset;
    uint8_t foreground_color;
    uint8_t background_color;
    uint8_t bold;
    uint8_t inverse;
    uint32_t line_buffers[TERMINAL_BUFFER_LINES];
} terminal_state_t;

/* terminal command handler */
typedef int (*terminal_command_func_t)(int argc, char** argv);

/* terminal command structure */
typedef struct {
    const char* name;
    const char* description;
    terminal_command_func_t handler;
} terminal_command_t;

/* terminal initialization */
int terminal_init(void);
void terminal_draw(void);
void terminal_clear(void);

/* terminal input handling */
void terminal_handle_keypress(uint8_t key);
void terminal_handle_special_key(uint8_t special_key);
void terminal_handle_enter(void);
void terminal_write_char(uint8_t ch);
void terminal_write_string(const char* str);

/* terminal output functions */
void terminal_puts(const char* str);
void terminal_printf(const char* format, ...);
void terminal_set_foreground_color(uint8_t color);
void terminal_set_background_color(uint8_t color);
void terminal_reset_colors(void);

/* terminal cursor control */
void terminal_move_cursor(uint32_t x, uint32_t y);
void terminal_get_cursor(uint32_t* x, uint32_t* y);
void terminal_save_cursor(void);
void terminal_restore_cursor(void);
void terminal_hide_cursor(void);
void terminal_show_cursor(void);

/* terminal scrolling */
void terminal_scroll_up(uint32_t lines);
void terminal_scroll_down(uint32_t lines);
void terminal_scroll_to_line(uint32_t line);

/* terminal buffer management */
void terminal_save_buffer(void);
void terminal_restore_buffer(void);
void terminal_clear_buffer(void);

/* terminal colors and attributes */
void terminal_set_bold(uint8_t enabled);
void terminal_set_inverse(uint8_t enabled);

/* terminal prompt */
void terminal_set_prompt(const char* prompt);
const char* terminal_get_prompt(void);
void terminal_write_prompt(void);
void terminal_redraw_current_line(void);

/* terminal history */
int terminal_add_history(const char* command);
const char* terminal_get_history(int index);
int terminal_get_history_count(void);

/* terminal commands */
int terminal_register_command(const char* name, const char* description, 
                             terminal_command_func_t handler);
int terminal_execute_command(const char* command_line);
void terminal_print_help(void);

/* builtin commands */
int cmd_help(int argc, char** argv);
int cmd_clear(int argc, char** argv);
int cmd_memory(int argc, char** argv);
int cmd_cpu(int argc, char** argv);
int cmd_log(int argc, char** argv);
int cmd_exit(int argc, char** argv);

/* terminal debugging */
void terminal_print_state(void);
void terminal_log_state(void);

#endif /* TERMINAL_H */