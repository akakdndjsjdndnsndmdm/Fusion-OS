/*
 * dolphin.h - dolphin monolithic kernel public interface
 * 
 * public interface for dolphin monolithic kernel components
 * and integration with gecko microkernel
 */

#ifndef DOLPHIN_H
#define DOLPHIN_H

#include <stdint.h>
#include <stdarg.h>
#include "framebuffer.h"

/* dolphin initialization */
int dolphin_init(void);

/* terminal interface */
void dolphin_terminal_puts(const char* str);
void dolphin_terminal_printf(const char* format, ...);
void dolphin_terminal_clear(void);
void dolphin_terminal_handle_key(uint8_t key);

/* framebuffer interface */
framebuffer_config_t* dolphin_get_framebuffer_config(void);
void dolphin_framebuffer_clear(uint32_t color);
void dolphin_draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void dolphin_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void dolphin_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color);

/* system information */
void dolphin_get_system_info(void* buffer, size_t size);

/* memory management */
void* dolphin_alloc_memory(size_t size);
void dolphin_free_memory(void* memory);

/* process management */
int dolphin_create_process(void (*function)(void), const char* name);
void dolphin_exit_process(void);

/* inter-process communication */
int dolphin_send_message(void* destination, const char* message, uint32_t length);
int dolphin_receive_message(void* source, char* buffer, uint32_t* length);

/* service registration */
int dolphin_register_service(const char* service_name, void* service_handler);
void* dolphin_lookup_service(const char* service_name);

/* gecko microkernel integration */
void dolphin_use_gecko_service(const char* service_name);

/* error handling */
void dolphin_handle_error(const char* subsystem, const char* message);

/* debugging */
void dolphin_print_state(void);

#endif /* DOLPHIN_H */