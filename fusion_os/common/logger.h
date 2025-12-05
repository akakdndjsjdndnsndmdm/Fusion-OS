/*
 * logger.h - Debug logging system for Fusion OS
 * 
 * Implements a circular buffer logging system that both Gecko and Dolphin
 * can use for debugging and system tracing.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stdarg.h>

/* log levels */
#define LOG_LEVEL_DEBUG  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR  3
#define LOG_LEVEL_CRITICAL 4

/* log buffer configuration */
#define LOG_BUFFER_SIZE 16384           /* 16KB circular buffer */
#define LOG_MESSAGE_MAX 256             /* maximum message length */
#define LOG_MAX_SUBSYSTEMS 32           /* maximum subsystems */

/* log entry structure */
typedef struct log_entry {
    uint64_t timestamp;
    uint8_t level;
    char subsystem[16];
    char message[LOG_MESSAGE_MAX];
    uint16_t message_length;
} log_entry_t;

/* logging configuration */
typedef struct {
    uint8_t debug_enabled;
    uint8_t log_to_console;
    uint8_t log_to_buffer;
    uint8_t current_level;
} logger_config_t;

/* logger initialization */
void logger_init(void);
void logger_set_config(logger_config_t* config);

/* logging functions */
void logger_log(uint8_t level, const char* subsystem, const char* format, ...);
void logger_vlog(uint8_t level, const char* subsystem, const char* format, va_list args);

/* convenience macros */
#define LOG_DEBUG(subsystem, format, ...) \
    logger_log(LOG_LEVEL_DEBUG, subsystem, format, ##__VA_ARGS__)

#define LOG_INFO(subsystem, format, ...) \
    logger_log(LOG_LEVEL_INFO, subsystem, format, ##__VA_ARGS__)

#define LOG_WARNING(subsystem, format, ...) \
    logger_log(LOG_LEVEL_WARNING, subsystem, format, ##__VA_ARGS__)

#define LOG_ERROR(subsystem, format, ...) \
    logger_log(LOG_LEVEL_ERROR, subsystem, format, ##__VA_ARGS__)

#define LOG_CRITICAL(subsystem, format, ...) \
    logger_log(LOG_LEVEL_CRITICAL, subsystem, format, ##__VA_ARGS__)

/* log buffer access */
void logger_get_entries(log_entry_t* buffer, uint32_t max_entries, uint32_t* count);
void logger_clear_buffer(void);
uint32_t logger_get_buffer_size(void);

/* debugging control */
void logger_set_level(uint8_t level);
uint8_t logger_get_level(void);
void logger_enable_debug(void);
void logger_disable_debug(void);

#endif /* LOGGER_H */