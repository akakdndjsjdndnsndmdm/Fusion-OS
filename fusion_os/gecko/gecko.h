/*
 * gecko.h - Gecko microkernel public interface
 * 
 * This header defines the public interface for the Gecko microkernel,
 * which provides core services to the Dolphin monolithic kernel.
 */

#ifndef GECKO_H
#define GECKO_H

#include <stdint.h>
#include <stddef.h>
#include "../common/list.h"

/* gecko initialization and startup */
int gecko_init(void);
void gecko_start_scheduler(void);

/* memory management interfaces */
void* gecko_alloc_page(void);
void gecko_free_page(void* page);
void* gecko_alloc_pages(uint32_t count);
void gecko_free_pages(void* pages, uint32_t count);
void* gecko_alloc_kernel_memory(size_t size);
void gecko_free_kernel_memory(void* memory);
int gecko_map_virtual_memory(void* virtual_addr, void* physical_addr, uint32_t flags);
void gecko_unmap_virtual_memory(void* virtual_addr);

/* scheduler interfaces */
typedef void (*task_function_t)(void);
int gecko_create_task(task_function_t function, const char* name);
int gecko_create_thread(void* stack, size_t stack_size, task_function_t function);
void gecko_yield(void);
void gecko_schedule(void);
void gecko_set_priority(int task_id, uint8_t priority);
uint8_t gecko_get_priority(int task_id);

/* inter-process communication */
#define GECKO_MAX_MESSAGE_SIZE 1024
#define GECKO_MAX_MESSAGE_QUEUE 64

typedef struct {
    char message_data[GECKO_MAX_MESSAGE_SIZE];
    uint32_t message_length;
    uint32_t message_type;
    void* sender;
    void* receiver;
} gecko_message_t;

int gecko_send_message(void* destination, const char* message, uint32_t length);
int gecko_receive_message(void* source, char* buffer, uint32_t* length);
int gecko_register_message_handler(void* handler, const char* service_name);
void* gecko_lookup_service(const char* service_name);

/* terminal driver registration */
typedef void (*terminal_write_function_t)(const char* text, uint32_t length);
typedef char (*terminal_read_function_t)(void);

int gecko_register_terminal_driver(terminal_write_function_t write_func, 
                                   terminal_read_function_t read_func);
terminal_write_function_t gecko_get_terminal_write(void);
terminal_read_function_t gecko_get_terminal_read(void);

/* system information */
void gecko_get_system_info(void* info_buffer, size_t buffer_size);
uint64_t gecko_get_uptime(void);

/* debug logging */
void gecko_log_debug(const char* subsystem, const char* message);
void gecko_log_info(const char* subsystem, const char* message);
void gecko_log_warning(const char* subsystem, const char* message);
void gecko_log_error(const char* subsystem, const char* message);

#endif /* GECKO_H */