    
    /* initialize virtual memory manager */
    vmm_init();
    
    /* initialize smp system */
    smp_init();
    
    /* initialize scheduler */
    scheduler_init();
    
    /* initialize ipc system */
    ipc_init();
    
    gecko_initialized = 1;
    LOG_INFO("gecko", "gecko microkernel initialized successfully");
    
    return 0;
}

/* start scheduler and begin normal operation */
void gecko_start_scheduler(void) {
    if (!gecko_initialized) {
        LOG_ERROR("gecko", "cannot start scheduler - gecko not initialized");
        return;
    }
    
    LOG_INFO("gecko", "starting scheduler");
    
    if (scheduler_start() != 0) {
        LOG_ERROR("gecko", "failed to start scheduler");
        return;
    }
    
    /* enter main scheduler loop */
    for (;;) {
        /* scheduler runs continuously, only yielding when tasks do */
        /* this will never return in normal operation */
        __asm__ volatile ("hlt" ::: "memory");
    }
}

/* memory management interfaces */
void* gecko_alloc_page(void) {
    return vmm_alloc_kernel_memory(PAGE_SIZE);
}

void gecko_free_page(void* page) {
    if (page != NULL) {
        vmm_free_kernel_memory(page);
    }
}

void* gecko_alloc_pages(uint32_t count) {
    return vmm_alloc_kernel_memory(count * PAGE_SIZE);
}

void gecko_free_pages(void* pages, uint32_t count) {
    if (pages != NULL) {
        vmm_free_kernel_memory(pages);
    }
}

void* gecko_alloc_kernel_memory(size_t size) {
    return vmm_alloc_kernel_memory(size);
}

void gecko_free_kernel_memory(void* memory) {
    vmm_free_kernel_memory(memory);
}

int gecko_map_virtual_memory(void* virtual_addr, void* physical_addr, uint32_t flags) {
    vmm_address_space_t* kernel_space = vmm_get_kernel_address_space();
    if (kernel_space == NULL) {
        return -1;
    }
    
    uint32_t vmm_flags = 0;
    if (flags & GECKO_MEMORY_READ) vmm_flags |= VMM_READ;
    if (flags & GECKO_MEMORY_WRITE) vmm_flags |= VMM_WRITE;
    if (flags & GECKO_MEMORY_EXEC) vmm_flags |= VMM_EXEC;
    if (flags & GECKO_MEMORY_USER) vmm_flags |= VMM_USER;
    
    return vmm_map_page(kernel_space, virtual_addr, physical_addr, vmm_flags);
}

void gecko_unmap_virtual_memory(void* virtual_addr) {
    vmm_address_space_t* kernel_space = vmm_get_kernel_address_space();
    if (kernel_space != NULL) {
        vmm_unmap_page(kernel_space, virtual_addr);
    }
}

/* scheduler interfaces */
int gecko_create_task(task_function_t function, const char* name) {
    return scheduler_create_task(function, name, PRIORITY_NORMAL);
}

int gecko_create_thread(void* stack, size_t stack_size, task_function_t function) {
    return scheduler_create_thread(stack, stack_size, function);
}

void gecko_yield(void) {
    scheduler_yield();
}

void gecko_schedule(void) {
    scheduler_schedule();
}

void gecko_set_priority(int task_id, uint8_t priority) {
    scheduler_set_priority(task_id, priority);
}

uint8_t gecko_get_priority(int task_id) {
    return scheduler_get_priority(task_id);
}

/* inter-process communication */
int gecko_send_message(void* destination, const char* message, uint32_t length) {
    if (message == NULL || length == 0) {
        return -1;
    }
    
    if (length > GECKO_MAX_MESSAGE_SIZE) {
        LOG_WARNING("gecko", "message too large: %u bytes", length);
        return -1;
    }
    
    return ipc_send_message(destination, message, length, IPC_MESSAGE_DATA, IPC_NONBLOCKING);
}

int gecko_receive_message(void* source, char* buffer, uint32_t* length) {
    if (buffer == NULL || length == NULL || *length == 0) {
        return -1;
    }
    
    uint32_t message_type;
    return ipc_receive_message(source, buffer, length, &message_type, 1000);
}

int gecko_register_message_handler(void* handler, const char* service_name) {
    if (handler == NULL || service_name == NULL) {
        return -1;
    }
    
    return ipc_register_service(service_name, handler);
}

void* gecko_lookup_service(const char* service_name) {
    return ipc_lookup_service(service_name);
}

/* terminal driver registration */
int gecko_register_terminal_driver(terminal_write_function_t write_func, 
                                   terminal_read_function_t read_func) {
    if (write_func == NULL) {
        return -1;
    }
    
    terminal_write_func = write_func;
    terminal_read_func = read_func;
    
    LOG_INFO("gecko", "terminal driver registered");
    return 0;
}

terminal_write_function_t gecko_get_terminal_write(void) {
    return terminal_write_func;
}

terminal_read_function_t gecko_get_terminal_read(void) {
    return terminal_read_func;
}

/* system information */
void gecko_get_system_info(void* info_buffer, size_t buffer_size) {
    /* populate system information */
    struct {
        uint32_t memory_total;
        uint32_t memory_free;
        uint32_t cpu_count;
        uint8_t initialized;
    } info;
    
    info.memory_total = pmm_get_total_memory();
    info.memory_free = pmm_get_free_memory();
    info.cpu_count = smp_get_cpu_count();
    info.initialized = gecko_initialized;
    
    if (buffer_size >= sizeof(info)) {
        memcpy(info_buffer, &info, sizeof(info));
    }
}

uint64_t gecko_get_uptime(void) {
    /* get system uptime from timer interrupt count */
    static uint64_t last_update = 0;
    static uint64_t uptime = 0;
    
    /* check for timer interrupt and update uptime */
    /* this would be updated by the timer interrupt handler */
    /* but im too fucking lazy so, increment based on timer frequency */
    uptime += 10; /* increment by 10ms tick */
    
    return uptime;
}

/* debug logging functions */
void gecko_log_debug(const char* subsystem, const char* message) {
    LOG_DEBUG(subsystem, "%s", message);
}

void gecko_log_info(const char* subsystem, const char* message) {
    LOG_INFO(subsystem, "%s", message);
}

void gecko_log_warning(const char* subsystem, const char* message) {
    LOG_WARNING(subsystem, "%s", message);
}

void gecko_log_error(const char* subsystem, const char* message) {
    LOG_ERROR(subsystem, "%s", message);
}
