/*
 * dolphin.c - dolphin monolithic kernel initialization
 * 
 * initializes dolphin kernel components and registers services
 * with the gecko microkernel
 */

#include "dolphin.h"
#include "framebuffer.h"
#include "terminal.h"
#include "proggy_clean_font.h"
#include "../gecko/gecko.h"
#include "../gecko/scheduler.h"
#include "../common/logger.h"
#include "../common/string.h"

/* dolphin initialization state */
static int dolphin_initialized = 0;

/* terminal write function for gecko */
static void dolphin_terminal_write(const char* text, uint32_t length) {
    terminal_write_string(text);
}

/* terminal read function for gecko */
static char dolphin_terminal_read(void) {
    /* implement proper keyboard input handling */
    /* this would read from keyboard buffer or interrupt handler */
    /* for now, return null character */
    return '\0';
}

/* initialize dolphin kernel */
int dolphin_init(void) {
    if (dolphin_initialized) {
        return 0;
    }
    
    LOG_INFO("dolphin", "initializing dolphin monolithic kernel");
    
    /* initialize framebuffer graphics */
    if (framebuffer_init() != 0) {
        LOG_ERROR("dolphin", "failed to initialize framebuffer");
        return -1;
    }
    
    /* initialize proggy clean font */
    if (proggy_font_init() != 0) {
        LOG_ERROR("dolphin", "failed to initialize proggy clean font");
        return -1;
    }
    
    /* initialize terminal */
    if (terminal_init() != 0) {
        LOG_ERROR("dolphin", "failed to initialize terminal");
        return -1;
    }
    
    /* register terminal driver with gecko microkernel */
    if (gecko_register_terminal_driver(dolphin_terminal_write, dolphin_terminal_read) != 0) {
        LOG_ERROR("dolphin", "failed to register terminal driver");
        return -1;
    }
    
    dolphin_initialized = 1;
    LOG_INFO("dolphin", "dolphin monolithic kernel initialized successfully");
    
    return 0;
}

/* terminal interface functions */
void dolphin_terminal_puts(const char* str) {
    terminal_write_string(str);
}

void dolphin_terminal_printf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    terminal_write_string(buffer);
}

void dolphin_terminal_clear(void) {
    terminal_clear();
}

void dolphin_terminal_handle_key(uint8_t key) {
    terminal_handle_keypress(key);
}

/* framebuffer interface functions */
framebuffer_config_t* dolphin_get_framebuffer_config(void) {
    return framebuffer_get_config();
}

void dolphin_framebuffer_clear(uint32_t color) {
    framebuffer_clear(color);
}

void dolphin_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    framebuffer_draw_pixel(x, y, color);
}

void dolphin_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    framebuffer_draw_rect(x, y, width, height, color);
}

void dolphin_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color) {
    terminal_text_area_t area;
    area.x = x;
    area.y = y;
    area.width = 1000; /* large enough */
    area.height = 100;
    area.foreground_color = color;
    area.background_color = COLOR_BLACK;
    area.attributes = TEXT_NORMAL;
    
    terminal_text_draw_line(&area, 0, text);
}

/* system information functions */
void dolphin_get_system_info(void* buffer, size_t size) {
    gecko_get_system_info(buffer, size);
}

/* memory management functions */
void* dolphin_alloc_memory(size_t size) {
    return gecko_alloc_kernel_memory(size);
}

void dolphin_free_memory(void* memory) {
    gecko_free_kernel_memory(memory);
}

/* process management functions */
int dolphin_create_process(void (*function)(void), const char* name) {
    return gecko_create_task(function, name);
}

void dolphin_exit_process(void) {
    /* implement process termination */
    /* terminate current process and clean up resources */
    extern task_t* scheduler_get_current_task(void);
    task_t* current = scheduler_get_current_task();
    if (current != NULL) {
        extern int scheduler_terminate_task(uint32_t task_id);
        scheduler_terminate_task(current->task_id);
    }
}

/* inter-process communication */
int dolphin_send_message(void* destination, const char* message, uint32_t length) {
    return gecko_send_message(destination, message, length);
}

int dolphin_receive_message(void* source, char* buffer, uint32_t* length) {
    return gecko_receive_message(source, buffer, length);
}

/* service registration */
int dolphin_register_service(const char* service_name, void* service_handler) {
    return gecko_register_message_handler(service_handler, service_name);
}

void* dolphin_lookup_service(const char* service_name) {
    return gecko_lookup_service(service_name);
}

/* gecko microkernel integration */
void dolphin_use_gecko_service(const char* service_name) {
    void* service = gecko_lookup_service(service_name);
    if (service != NULL) {
        LOG_INFO("dolphin", "connected to gecko service: %s", service_name);
    } else {
        LOG_WARNING("dolphin", "gecko service not found: %s", service_name);
    }
}

/* error handling */
void dolphin_handle_error(const char* subsystem, const char* message) {
    LOG_ERROR(subsystem, "%s", message);
}

/* debugging functions */
void dolphin_print_state(void) {
    LOG_INFO("dolphin", "dolphin kernel state:");
    LOG_INFO("dolphin", "  initialized: %s", dolphin_initialized ? "yes" : "no");
    
    framebuffer_config_t* fb_config = framebuffer_get_config();
    if (fb_config != NULL) {
        LOG_INFO("dolphin", "  framebuffer: %ux%u at %dbpp", 
                fb_config->width, fb_config->height, fb_config->bits_per_pixel);
    }
}