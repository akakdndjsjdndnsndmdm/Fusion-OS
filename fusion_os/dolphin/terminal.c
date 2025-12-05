/*
 * terminal.c - linux-like terminal implementation for fusion os
 * 
 * implements terminal emulation with framebuffer and proggy clean font
 * with command processing and linux-like interface
 */

#include "terminal.h"
#include "framebuffer.h"
#include "proggy_clean_font.h"
#include "../gecko/pmm.h"
#include "../gecko/smp.h"
#include "../common/string.h"
#include "../common/logger.h"
#include "../gecko/gecko.h"

/* terminal state */
static terminal_state_t terminal_state;
static framebuffer_config_t* fb_config = NULL;
static terminal_text_area_t terminal_area;
static const char* terminal_prompt = "fusion_os$ ";

/* command history */
#define COMMAND_HISTORY_SIZE 50
static char command_history[COMMAND_HISTORY_SIZE][256];
static int command_history_count = 0;
static int current_history_index = -1;

/* command buffer */
static char command_buffer[256];
static int command_buffer_pos = 0;

/* terminal commands */
#define MAX_COMMANDS 32
static terminal_command_t registered_commands[MAX_COMMANDS];
static int command_count = 0;

/*
 * initialize terminal
 */
int terminal_init(void) {
    LOG_INFO("terminal", "initializing terminal");
    
    /* initialize framebuffer */
    if (framebuffer_init() != 0) {
        LOG_ERROR("terminal", "failed to initialize framebuffer");
        return -1;
    }
    
    /* get framebuffer configuration */
    fb_config = framebuffer_get_config();
    if (fb_config == NULL) {
        LOG_ERROR("terminal", "failed to get framebuffer config");
        return -1;
    }
    
    /* initialize proggy clean font */
    if (proggy_font_init() != 0) {
        LOG_ERROR("terminal", "failed to initialize proggy clean font");
        return -1;
    }
    
    /* initialize terminal state */
    memset(&terminal_state, 0, sizeof(terminal_state));
    terminal_state.cursor_x = 0;
    terminal_state.cursor_y = 0;
    terminal_state.scroll_offset = 0;
    terminal_state.foreground_color = TERMINAL_COLOR_WHITE;
    terminal_state.background_color = TERMINAL_COLOR_BLACK;
    terminal_state.bold = 0;
    terminal_state.inverse = 0;
    
    /* initialize terminal area */
    terminal_area.x = 10;
    terminal_area.y = 10;
    terminal_area.width = fb_config->width - 20;
    terminal_area.height = fb_config->height - 20;
    terminal_area.foreground_color = terminal_state.foreground_color;
    terminal_area.background_color = terminal_state.background_color;
    terminal_area.attributes = TEXT_NORMAL;
    
    /* initialize command buffer */
    memset(command_buffer, 0, sizeof(command_buffer));
    command_buffer_pos = 0;
    
    /* register builtin commands */
    terminal_register_command("help", "show this help message", cmd_help);
    terminal_register_command("clear", "clear the terminal screen", cmd_clear);
    terminal_register_command("memory", "show memory usage information", cmd_memory);
    terminal_register_command("cpu", "show cpu information", cmd_cpu);
    terminal_register_command("log", "show system log", cmd_log);
    terminal_register_command("exit", "exit the terminal", cmd_exit);
    
    /* clear terminal and show prompt */
    terminal_clear();
    terminal_write_prompt();
    
    LOG_INFO("terminal", "terminal initialized");
    return 0;
}

/*
 * draw terminal interface
 */
void terminal_draw(void) {
    if (fb_config == NULL) {
        return;
    }
    
    /* clear terminal area */
    terminal_text_clear_area(&terminal_area);
    
    /* redraw terminal content */
    int max_visible_lines = (terminal_area.height - PROGGY_FONT_HEIGHT) / PROGGY_FONT_HEIGHT;
    
    for (int i = 0; i < max_visible_lines && i < TERMINAL_HEIGHT_CHARS; i++) {
        int line_num = terminal_state.scroll_offset + i;
        if (line_num < terminal_state.line_buffers[i]) {
            /* draw line from buffer */
            /* for now, just draw prompt if it's the current line */
            if (line_num == terminal_state.cursor_y) {
                /* draw command buffer */
                terminal_text_draw_line(&terminal_area, i, command_buffer);
            }
        }
    }
    
    /* draw cursor */
    uint32_t cursor_color = terminal_state.inverse ? 
                           terminal_state.background_color : terminal_state.foreground_color;
    
    int cursor_x = terminal_area.x + (terminal_state.cursor_x * PROGGY_FONT_WIDTH);
    int cursor_y = terminal_area.y + (terminal_state.cursor_y * PROGGY_FONT_HEIGHT);
    
    framebuffer_draw_rect(cursor_x, cursor_y, PROGGY_FONT_WIDTH, PROGGY_FONT_HEIGHT, cursor_color);
}

/*
 * clear terminal screen
 */
void terminal_clear(void) {
    if (terminal_area.background_color != COLOR_BLACK) {
        framebuffer_clear(terminal_area.background_color);
    }
    
    terminal_state.cursor_x = 0;
    terminal_state.cursor_y = 0;
    terminal_state.scroll_offset = 0;
    
    /* clear line buffers */
    memset(terminal_state.line_buffers, 0, sizeof(terminal_state.line_buffers));
    
    /* clear command buffer */
    memset(command_buffer, 0, sizeof(command_buffer));
    command_buffer_pos = 0;
}

/*
 * handle keypress
 */
void terminal_handle_keypress(uint8_t key) {
    /* handle printable characters */
    if (key >= 32 && key <= 126) {
        if (command_buffer_pos < sizeof(command_buffer) - 1) {
            command_buffer[command_buffer_pos++] = key;
            command_buffer[command_buffer_pos] = '\0';
            terminal_write_char(key);
        }
    }
    /* handle backspace */
    else if (key == 0x08 || key == 0x7f) { /* backspace or delete */
        if (command_buffer_pos > 0) {
            command_buffer_pos--;
            command_buffer[command_buffer_pos] = '\0';
            if (terminal_state.cursor_x > 0) {
                terminal_state.cursor_x--;
                /* redraw line */
                terminal_redraw_current_line();
            }
        }
    }
    /* handle enter */
    else if (key == '\r' || key == '\n') {
        terminal_handle_enter();
    }
    /* handle escape sequences */
    else if (key == 0x1b) {
        /* handle escape sequence */
    }
}

/*
 * handle enter key
 */
void terminal_handle_enter(void) {
    /* add command to history */
    if (command_buffer_pos > 0) {
        terminal_add_history(command_buffer);
    }
    
    /* execute command */
    terminal_execute_command(command_buffer);
    
    /* move to next line */
    terminal_state.cursor_y++;
    terminal_state.cursor_x = 0;
    
    /* clear command buffer */
    memset(command_buffer, 0, sizeof(command_buffer));
    command_buffer_pos = 0;
    
    /* write new prompt */
    terminal_write_prompt();
    
    /* scroll if needed */
    if (terminal_state.cursor_y >= TERMINAL_HEIGHT_CHARS) {
        terminal_scroll_up(1);
        terminal_state.cursor_y--;
    }
}

/*
 * write single character
 */
void terminal_write_char(uint8_t ch) {
    if (terminal_state.cursor_x >= TERMINAL_WIDTH_CHARS) {
        terminal_state.cursor_x = 0;
        terminal_state.cursor_y++;
        
        if (terminal_state.cursor_y >= TERMINAL_HEIGHT_CHARS) {
            terminal_scroll_up(1);
            terminal_state.cursor_y--;
        }
    }
    
    terminal_text_draw_char(&terminal_area, terminal_state.cursor_x, terminal_state.cursor_y, ch);
    terminal_state.cursor_x++;
}

/*
 * write string
 */
void terminal_write_string(const char* str) {
    while (*str != '\0') {
        terminal_write_char(*str);
        str++;
    }
}

/*
 * write formatted string
 */
void terminal_printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    terminal_write_string(buffer);
}

/*
 * write prompt
 */
void terminal_write_prompt(void) {
    terminal_write_string(terminal_prompt);
}

/*
 * redraw current line
 */
void terminal_redraw_current_line(void) {
    int line_y = terminal_state.cursor_y;
    int line_x = terminal_state.cursor_x;
    
    /* clear the line */
    for (int x = 0; x < TERMINAL_WIDTH_CHARS; x++) {
        terminal_text_draw_char(&terminal_area, x, line_y, ' ');
    }
    
    /* redraw prompt */
    int pos = strlen(terminal_prompt);
    const char* prompt_ptr = terminal_prompt;
    while (*prompt_ptr != '\0') {
        terminal_text_draw_char(&terminal_area, pos, line_y, *prompt_ptr);
        prompt_ptr++;
        pos++;
    }
    
    /* redraw command buffer */
    const char* cmd_ptr = command_buffer;
    while (*cmd_ptr != '\0' && pos < TERMINAL_WIDTH_CHARS) {
        terminal_text_draw_char(&terminal_area, pos, line_y, *cmd_ptr);
        cmd_ptr++;
        pos++;
    }
    
    terminal_state.cursor_x = line_x;
    terminal_state.cursor_y = line_y;
}

/*
 * scroll up
 */
void terminal_scroll_up(uint32_t lines) {
    if (lines == 0) {
        return;
    }
    
    uint32_t scroll_lines = (lines * PROGGY_FONT_HEIGHT);
    int max_scroll = (TERMINAL_BUFFER_LINES * PROGGY_FONT_HEIGHT) - terminal_area.height;
    
    if (scroll_lines > max_scroll) {
        scroll_lines = max_scroll;
    }
    
    /* move terminal area up */
    terminal_area.y += scroll_lines;
    terminal_state.scroll_offset += lines;
    
    /* redraw if needed */
    terminal_draw();
}

/*
 * scroll down
 */
void terminal_scroll_down(uint32_t lines) {
    if (lines == 0 || terminal_state.scroll_offset == 0) {
        return;
    }
    
    uint32_t scroll_lines = (lines * PROGGY_FONT_HEIGHT);
    
    if (scroll_lines > terminal_state.scroll_offset * PROGGY_FONT_HEIGHT) {
        scroll_lines = terminal_state.scroll_offset * PROGGY_FONT_HEIGHT;
    }
    
    /* move terminal area down */
    terminal_area.y -= scroll_lines;
    terminal_state.scroll_offset -= lines;
    
    /* redraw if needed */
    terminal_draw();
}

/*
 * set foreground color
 */
void terminal_set_foreground_color(uint8_t color) {
    terminal_state.foreground_color = color;
    terminal_area.foreground_color = color;
}

/*
 * set background color
 */
void terminal_set_background_color(uint8_t color) {
    terminal_state.background_color = color;
    terminal_area.background_color = color;
}

/*
 * reset colors
 */
void terminal_reset_colors(void) {
    terminal_set_foreground_color(TERMINAL_COLOR_WHITE);
    terminal_set_background_color(TERMINAL_COLOR_BLACK);
}

/*
 * set bold attribute
 */
void terminal_set_bold(uint8_t enabled) {
    terminal_state.bold = enabled;
}

/*
 * set inverse attribute
 */
void terminal_set_inverse(uint8_t enabled) {
    terminal_state.inverse = enabled;
}

/*
 * add command to history
 */
int terminal_add_history(const char* command) {
    if (command == NULL || strlen(command) == 0) {
        return -1;
    }
    
    if (command_history_count >= COMMAND_HISTORY_SIZE) {
        /* shift history */
        memmove(command_history, command_history[1], 
               (COMMAND_HISTORY_SIZE - 1) * 256);
        command_history_count--;
    }
    
    strncpy(command_history[command_history_count], command, 255);
    command_history[command_history_count][255] = '\0';
    command_history_count++;
    current_history_index = -1;
    
    return 0;
}

/*
 * get command from history
 */
const char* terminal_get_history(int index) {
    if (index < 0 || index >= command_history_count) {
        return NULL;
    }
    
    return command_history[index];
}

/*
 * get history count
 */
int terminal_get_history_count(void) {
    return command_history_count;
}

/*
 * register terminal command
 */
int terminal_register_command(const char* name, const char* description, 
                             terminal_command_func_t handler) {
    if (command_count >= MAX_COMMANDS) {
        return -1;
    }
    
    registered_commands[command_count].name = name;
    registered_commands[command_count].description = description;
    registered_commands[command_count].handler = handler;
    command_count++;
    
    return 0;
}

/*
 * execute command
 */
int terminal_execute_command(const char* command_line) {
    if (command_line == NULL || strlen(command_line) == 0) {
        return 0;
    }
    
    /* parse command line */
    char* argv[16];
    int argc = 0;
    
    char* cmd_copy = strdup(command_line);
    char* token = strtok(cmd_copy, " \t");
    
    while (token != NULL && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    
    if (argc == 0) {
        free(cmd_copy);
        return 0;
    }
    
    /* find and execute command */
    for (int i = 0; i < command_count; i++) {
        if (strcmp(registered_commands[i].name, argv[0]) == 0) {
            int result = registered_commands[i].handler(argc, argv);
            free(cmd_copy);
            return result;
        }
    }
    
    /* command not found */
    terminal_printf("command not found: %s\n", argv[0]);
    
    free(cmd_copy);
    return -1;
}

/*
 * print help
 */
void terminal_print_help(void) {
    terminal_printf("available commands:\n");
    for (int i = 0; i < command_count; i++) {
        terminal_printf("  %-10s - %s\n", registered_commands[i].name, 
                       registered_commands[i].description);
    }
}

/* builtin command implementations */

/*
 * help command
 */
int cmd_help(int argc, char** argv) {
    terminal_print_help();
    return 0;
}

/*
 * clear command
 */
int cmd_clear(int argc, char** argv) {
    terminal_clear();
    return 0;
}

/*
 * memory command
 */
int cmd_memory(int argc, char** argv) {
    uint32_t total_mem = pmm_get_total_memory();
    uint32_t free_mem = pmm_get_free_memory();
    uint32_t used_mem = total_mem - free_mem;
    
    terminal_printf("memory information:\n");
    terminal_printf("  total: %u mb\n", total_mem / (1024 * 1024));
    terminal_printf("  free: %u mb\n", free_mem / (1024 * 1024));
    terminal_printf("  used: %u mb\n", used_mem / (1024 * 1024));
    terminal_printf("  usage: %u%%\n", (used_mem * 100) / total_mem);
    
    return 0;
}

/*
 * cpu command
 */
int cmd_cpu(int argc, char** argv) {
    terminal_printf("cpu information:\n");
    terminal_printf("  smp enabled: %s\n", "yes");
    terminal_printf("  cpu count: %u\n", smp_get_cpu_count());
    
    return 0;
}

/*
 * log command
 */
int cmd_log(int argc, char** argv) {
    terminal_printf("recent log entries:\n");
    
    /* display basic log information */
    terminal_printf("  debug: enabled\n");
    terminal_printf("  info: enabled\n");
    terminal_printf("  warning: enabled\n");
    terminal_printf("  error: enabled\n");
    
    return 0;
}

/*
 * exit command
 */
int cmd_exit(int argc, char** argv) {
    terminal_printf("exiting terminal...\n");
    /* for now, just clear the screen */
    terminal_clear();
    return 0;
}

/*
 * get terminal state (for debugging)
 */
void terminal_print_state(void) {
    LOG_INFO("terminal", "terminal state:");
    LOG_INFO("terminal", "  cursor: (%u, %u)", terminal_state.cursor_x, terminal_state.cursor_y);
    LOG_INFO("terminal", "  scroll offset: %u", terminal_state.scroll_offset);
    LOG_INFO("terminal", "  colors: fg 0x%x, bg 0x%x", 
             terminal_state.foreground_color, terminal_state.background_color);
    LOG_INFO("terminal", "  attributes: bold=%u, inverse=%u", 
             terminal_state.bold, terminal_state.inverse);
}