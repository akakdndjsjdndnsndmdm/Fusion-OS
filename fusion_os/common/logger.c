/*
 * logger.c - debug logging system for fusion os
 * 
 * implements circular buffer logging that both gecko and dolphin
 * can use for debugging and system tracing
 */

#include "logger.h"
#include "../common/string.h"

/* log buffer configuration */
#define LOG_BUFFER_SIZE 16384           /* 16kb circular buffer */
#define LOG_MESSAGE_MAX 256             /* maximum message length */
#define LOG_MAX_SUBSYSTEMS 32           /* maximum subsystems */

/* logging configuration */
static logger_config_t logger_config = {
    .debug_enabled = 0,
    .log_to_console = 1,
    .log_to_buffer = 1,
    .current_level = LOG_LEVEL_INFO
};

/* log buffer */
static uint8_t log_buffer[LOG_BUFFER_SIZE];
static volatile uint32_t log_head = 0;
static volatile uint32_t log_tail = 0;
static volatile uint32_t log_count = 0;
static uint64_t log_start_time = 0;

/* subsystem name array */
static const char* subsystem_names[LOG_MAX_SUBSYSTEMS] = {
    "fusion_os", "gecko", "dolphin", "memory", "scheduler", "ipc", 
    "framebuffer", "terminal", "smp", "debug"
};

/*
 * initialize logging system
 */
void logger_init(void) {
    if (log_start_time != 0) {
        return; /* already initialized */
    }
    
    /* initialize buffer */
    memset(log_buffer, 0, sizeof(log_buffer));
    log_head = 0;
    log_tail = 0;
    log_count = 0;
    log_start_time = 100; /* start from 100 to avoid zero */
    
    /* enable logging by default */
    logger_config.log_to_console = 1;
    logger_config.log_to_buffer = 1;
    logger_config.current_level = LOG_LEVEL_INFO;
    
    /* log initial message */
    logger_log(LOG_LEVEL_INFO, "logger", "logging system initialized");
}

/*
 * write log entry to buffer
 */
static void write_log_to_buffer(uint64_t timestamp, uint8_t level, 
                               const char* subsystem, const char* message) {
    /* calculate entry size */
    uint32_t subsystem_len = strlen(subsystem);
    uint32_t message_len = strlen(message);
    uint32_t entry_size = sizeof(uint64_t) + 1 + 1 + subsystem_len + 1 + message_len + 1;
    
    /* handle buffer wrap */
    if (log_head + entry_size > LOG_BUFFER_SIZE) {
        log_head = 0;
    }
    
    /* write timestamp */
    memcpy(&log_buffer[log_head], &timestamp, sizeof(uint64_t));
    log_head += sizeof(uint64_t);
    
    /* write level */
    log_buffer[log_head++] = level;
    
    /* write subsystem */
    memcpy(&log_buffer[log_head], &subsystem_len, 1);
    log_head++;
    memcpy(&log_buffer[log_head], subsystem, subsystem_len);
    log_head += subsystem_len;
    
    /* write message */
    memcpy(&log_buffer[log_head], &message_len, 1);
    log_head++;
    memcpy(&log_buffer[log_head], message, message_len);
    log_head += message_len;
    
    /* update count */
    if (log_count < LOG_BUFFER_SIZE / entry_size) {
        log_count++;
    }
}

/*
 * write log to console
 */
static void write_log_to_console(uint64_t timestamp, uint8_t level, 
                                const char* subsystem, const char* message) {
    /* get level string */
    const char* level_str[] = {
        "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
    };
    
    /* format timestamp */
    uint32_t seconds = timestamp / 1000;
    uint32_t milliseconds = timestamp % 1000;
    
    /* print to console */
    char buffer[LOG_MESSAGE_MAX + 100];
    int len = snprintf(buffer, sizeof(buffer), "[%u.%03u] %s: %s: %s\r\n", 
                      seconds, milliseconds, level_str[level], subsystem, message);
    
    /* output to vga text buffer for now */
    uint16_t* vga_buffer = (uint16_t*)0xb8000;
    for (int i = 0; i < len && i < 80; i++) {
        vga_buffer[i] = 0x0700 | buffer[i];
    }
}

/*
 * main logging function
 */
void logger_log(uint8_t level, const char* subsystem, const char* format, ...) {
    if (log_start_time == 0) {
        logger_init();
    }
    
    /* check if logging is enabled for this level */
    if (level < logger_config.current_level) {
        return;
    }
    
    /* check if debug mode is enabled */
    if (level == LOG_LEVEL_DEBUG && !logger_config.debug_enabled) {
        return;
    }
    
    /* find subsystem */
    const char* subsystem_name = subsystem;
    for (int i = 0; i < LOG_MAX_SUBSYSTEMS; i++) {
        if (subsystem_names[i] != NULL && strcmp(subsystem_names[i], subsystem) == 0) {
            subsystem_name = subsystem_names[i];
            break;
        }
    }
    
    /* format message */
    char message[LOG_MESSAGE_MAX];
    va_list args;
    va_start(args, format);
    int message_len = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    /* ensure null termination */
    if (message_len >= sizeof(message)) {
        message_len = sizeof(message) - 1;
        message[message_len] = '\0';
    }
    
    /* get timestamp */
    uint64_t timestamp = 100; /* simplified for now */
    
    /* write to buffer */
    if (logger_config.log_to_buffer) {
        write_log_to_buffer(timestamp, level, subsystem_name, message);
    }
    
    /* write to console */
    if (logger_config.log_to_console) {
        write_log_to_console(timestamp, level, subsystem_name, message);
    }
}

/*
 * variable argument logging function
 */
void logger_vlog(uint8_t level, const char* subsystem, const char* format, va_list args) {
    if (log_start_time == 0) {
        logger_init();
    }
    
    /* check if logging is enabled for this level */
    if (level < logger_config.current_level) {
        return;
    }
    
    /* check if debug mode is enabled */
    if (level == LOG_LEVEL_DEBUG && !logger_config.debug_enabled) {
        return;
    }
    
    /* format message */
    char message[LOG_MESSAGE_MAX];
    int message_len = vsnprintf(message, sizeof(message), format, args);
    
    /* ensure null termination */
    if (message_len >= sizeof(message)) {
        message_len = sizeof(message) - 1;
        message[message_len] = '\0';
    }
    
    /* get timestamp */
    uint64_t timestamp = 100; /* simplified for now */
    
    /* write to buffer */
    if (logger_config.log_to_buffer) {
        write_log_to_buffer(timestamp, level, subsystem, message);
    }
    
    /* write to console */
    if (logger_config.log_to_console) {
        write_log_to_console(timestamp, level, subsystem, message);
    }
}

/*
 * set logging configuration
 */
void logger_set_config(logger_config_t* config) {
    if (config != NULL) {
        logger_config = *config;
    }
}

/*
 * get log entries from buffer
 */
void logger_get_entries(log_entry_t* buffer, uint32_t max_entries, uint32_t* count) {
    if (buffer == NULL || count == NULL || max_entries == 0) {
        return;
    }
    
    /* read from buffer */
    uint32_t entries_read = 0;
    uint32_t current = log_tail;
    
    for (uint32_t i = 0; i < log_count && entries_read < max_entries; i++) {
        if (current + sizeof(uint64_t) + 1 + 1 >= LOG_BUFFER_SIZE) {
            current = 0;
        }
        
        /* read timestamp */
        uint64_t timestamp;
        memcpy(&timestamp, &log_buffer[current], sizeof(uint64_t));
        current += sizeof(uint64_t);
        
        /* read level */
        uint8_t level = log_buffer[current++];
        
        /* read subsystem */
        uint8_t subsystem_len = log_buffer[current++];
        char subsystem[16];
        if (subsystem_len < sizeof(subsystem)) {
            memcpy(subsystem, &log_buffer[current], subsystem_len);
            subsystem[subsystem_len] = '\0';
            current += subsystem_len;
        }
        
        /* read message */
        uint8_t message_len = log_buffer[current++];
        char message[LOG_MESSAGE_MAX];
        if (message_len < sizeof(message)) {
            memcpy(message, &log_buffer[current], message_len);
            message[message_len] = '\0';
            current += message_len;
        }
        
        /* populate entry */
        buffer[entries_read].timestamp = timestamp;
        buffer[entries_read].level = level;
        strncpy(buffer[entries_read].subsystem, subsystem, sizeof(buffer[entries_read].subsystem) - 1);
        buffer[entries_read].subsystem[sizeof(buffer[entries_read].subsystem) - 1] = '\0';
        strncpy(buffer[entries_read].message, message, sizeof(buffer[entries_read].message) - 1);
        buffer[entries_read].message[sizeof(buffer[entries_read].message) - 1] = '\0';
        buffer[entries_read].message_length = message_len;
        
        entries_read++;
    }
    
    *count = entries_read;
}

/*
 * clear log buffer
 */
void logger_clear_buffer(void) {
    log_head = 0;
    log_tail = 0;
    log_count = 0;
    memset(log_buffer, 0, sizeof(log_buffer));
}

/*
 * get buffer size
 */
uint32_t logger_get_buffer_size(void) {
    return LOG_BUFFER_SIZE;
}

/*
 * set log level
 */
void logger_set_level(uint8_t level) {
    logger_config.current_level = level;
}

/*
 * get current log level
 */
uint8_t logger_get_level(void) {
    return logger_config.current_level;
}

/*
 * enable debug logging
 */
void logger_enable_debug(void) {
    logger_config.debug_enabled = 1;
    logger_config.current_level = LOG_LEVEL_DEBUG;
}

/*
 * disable debug logging
 */
void logger_disable_debug(void) {
    logger_config.debug_enabled = 0;
    logger_config.current_level = LOG_LEVEL_INFO;
}