/*
 * fusion_os.c - Main entry point and system initialization
 * 
 * This is the main C entry point for Fusion OS, called from boot assembly.
 * It initializes both Gecko (microkernel) and Dolphin (monolithic) components.
 */

#include "gecko/gecko.h"
#include "dolphin/dolphin.h"
#include "common/logger.h"
#include "common/string.h"

/* system initialization flags */
static uint8_t system_initialized = 0;

/*
 * main entry point called from boot assembly
 * initializes microkernel first, then monolithic components
 */
void kernel_main(void) {
    /* initialize logging system first */
    logger_init();
    
    LOG_INFO("fusion_os", "starting fusion os initialization...");
    
    /* initialize gecko microkernel */
    if (gecko_init() != 0) {
        LOG_ERROR("fusion_os", "gecko microkernel initialization failed");
        return;
    }
    
    /* initialize dolphin monolithic kernel */
    if (dolphin_init() != 0) {
        LOG_ERROR("fusion_os", "dolphin monolithic kernel initialization failed");
        return;
    }
    
    system_initialized = 1;
    LOG_INFO("fusion_os", "fusion os initialization complete");
    
    /* start the system scheduler */
    gecko_start_scheduler();
    
    /* if we get here, something went wrong */
    LOG_ERROR("fusion_os", "scheduler returned - system error");
}

/*
 * check if system is fully initialized
 */
uint8_t system_is_initialized(void) {
    return system_initialized;
}

/*
 * entry point for linker - calls kernel_main
 */
void _start(void) {
    kernel_main();
    /* kernel_main should never return */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
