/*
 * smp.c - symmetric multiprocessing implementation for fusion os
 * 
 * implements smp with local apic and io apic support for multi-core systems
 */

#include "smp.h"
#include "../common/logger.h"
#include "../common/string.h"

/* global smp configuration */
static smp_config_t smp_config;
static int smp_initialized = 0;
static uint8_t current_cpu_id = 0;

/* apic access functions */
static inline uint32_t read_local_apic(uint32_t offset) {
    return *((volatile uint32_t*)(LOCAL_APIC_BASE + offset));
}

static inline void write_local_apic(uint32_t offset, uint32_t value) {
    *((volatile uint32_t*)(LOCAL_APIC_BASE + offset)) = value;
}

static inline uint32_t read_io_apic(uint32_t offset) {
    return *((volatile uint32_t*)(IO_APIC_BASE + offset));
}

static inline void write_io_apic(uint32_t offset, uint32_t value) {
    *((volatile uint32_t*)(IO_APIC_BASE + offset)) = value;
}

/*
 * detect cpu cores using cpuid
 */
static int detect_cpus_with_cpuid(void) {
    uint32_t eax, ebx, ecx, edx;
    
    /* check for apic support */
    __asm__ volatile (
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (1)
    );
    
    if (!(edx & (1 << 9))) { /* check for apic bit */
        LOG_WARNING("smp", "cpu apic not supported");
        return -1;
    }
    
    /* get cpu count */
    __asm__ volatile (
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (0x1)
    );
    
    smp_config.cpu_count = ((ebx >> 16) & 0xff) + 1; /* logical processors per core */
    
    LOG_INFO("smp", "detected %u logical processors", smp_config.cpu_count);
    return 0;
}

/*
 * initialize local apic for current cpu
 */
int smp_init_local_apic(uint8_t cpu_id) {
    uint32_t spurious_vector = 0xff;
    
    /* enable local apic */
    write_local_apic(LOCAL_APIC_SPURIOUS_INTERRUPT_VECTOR, spurious_vector | 0x100);
    
    /* clear error status */
    write_local_apic(LOCAL_APIC_ESR, 0);
    
    /* clear previous timer interrupt */
    write_local_apic(LOCAL_APIC_EOI, 0);
    
    LOG_INFO("smp", "local apic initialized for cpu %u", cpu_id);
    return 0;
}

/*
 * initialize io apic
 */
int smp_init_io_apic(void) {
    /* get io apic version */
    uint32_t version = read_io_apic(IO_APIC_VERSION);
    uint8_t max_redirection_entries = ((version >> 16) & 0xff) + 1;
    
    LOG_INFO("smp", "io apic version %u.%u with %u redirection entries",
             (version >> 4) & 0xf, version & 0xf, max_redirection_entries);
    
    /* mask all interrupts initially */
    for (int i = 0; i < max_redirection_entries && i < 24; i++) {
        uint32_t redirection = (i << 16) | 0x10000; /* vector 0, masked */
        write_io_apic(0x10 + (i * 2), redirection);
    }
    
    return 0;
}

/*
 * send inter-processor interrupt
 */
void smp_send_ipi(uint8_t target_cpu, uint8_t vector) {
    if (target_cpu >= smp_config.cpu_count) {
        return;
    }
    
    uint32_t destination = smp_config.cpus[target_cpu].apic_id;
    uint32_t icr = (vector << 24) | (destination << 12) | 0x4000; /* fixed delivery mode */
    
    write_local_apic(0x310, icr);
    
    /* wait for delivery */
    while (read_local_apic(0x300) & (1 << 12)) {
        /* busy wait */
    }
}

/*
 * send broadcast ipi to all other cpus
 */
void smp_send_broadcast_ipi(uint8_t vector) {
    uint32_t icr = (vector << 24) | 0x8000; /* broadcast to all except self */
    
    write_local_apic(0x310, icr);
    
    /* wait for delivery */
    while (read_local_apic(0x300) & (1 << 12)) {
        /* busy wait */
    }
}

/*
 * setup local apic timer
 */
void smp_setup_timer(uint8_t cpu_id, uint32_t frequency_hz) {
    uint32_t initial_count = 0xffffffff / frequency_hz; /* calculate initial count */
    
    /* set timer divide value */
    write_local_apic(LOCAL_APIC_TIMER_DIVIDE, 0x03); /* divide by 16 */
    
    /* set initial count */
    write_local_apic(LOCAL_APIC_TIMER_INITIAL_COUNT, initial_count);
    
    /* enable timer interrupt */
    uint32_t timer_vector = 0x80 + cpu_id; /* timer vectors per cpu */
    write_local_apic(LOCAL_APIC_TIMER, timer_vector | 0x20000); /* periodic mode */
    
    LOG_INFO("smp", "timer setup for cpu %u at %u hz", cpu_id, frequency_hz);
}

/*
 * start local apic timer
 */
void smp_start_timer(uint8_t cpu_id) {
    uint32_t initial_count = 0xffffffff;
    write_local_apic(LOCAL_APIC_TIMER_INITIAL_COUNT, initial_count);
}

/*
 * stop local apic timer
 */
void smp_stop_timer(uint8_t cpu_id) {
    write_local_apic(LOCAL_APIC_TIMER_INITIAL_COUNT, 0);
}

/*
 * enable interrupts through apic
 */
void smp_enable_interrupts(void) {
    uint32_t tpr = read_local_apic(LOCAL_APIC_TASK_PRIORITY);
    write_local_apic(LOCAL_APIC_TASK_PRIORITY, tpr & ~0xff);
}

/*
 * disable interrupts through apic
 */
void smp_disable_interrupts(void) {
    uint32_t tpr = read_local_apic(LOCAL_APIC_TASK_PRIORITY);
    write_local_apic(LOCAL_APIC_TASK_PRIORITY, tpr | 0xff);
}

/*
 * route interrupt to specific cpu
 */
int smp_route_interrupt(uint8_t interrupt, uint8_t cpu_id, uint8_t vector) {
    if (interrupt >= 24 || cpu_id >= smp_config.cpu_count) {
        return -1;
    }
    
    uint32_t redirection = (vector << 24) | 
                          (smp_config.cpus[cpu_id].apic_id << 12) |
                          0x800; /* edge-triggered */
    
    write_io_apic(0x10 + (interrupt * 2), redirection);
    
    LOG_DEBUG("smp", "routed interrupt %u to cpu %u with vector %u", 
             interrupt, cpu_id, vector);
    return 0;
}

/*
 * get current cpu id
 */
uint8_t smp_get_current_cpu_id(void) {
    return current_cpu_id;
}

/*
 * get current cpu apic id
 */
uint8_t smp_get_current_cpu_apic_id(void) {
    return (read_local_apic(LOCAL_APIC_ID) >> 24) & 0xff;
}

/*
 * check if cpu is active
 */
int smp_cpu_is_active(uint8_t cpu_id) {
    if (cpu_id >= smp_config.cpu_count) {
        return 0;
    }
    
    return (smp_config.cpus[cpu_id].flags & 0x01) != 0;
}

/*
 * memory barriers for smp
 */
void smp_memory_barrier(void) {
    __asm__ volatile ("mfence" ::: "memory");
}

void smp_read_barrier(void) {
    __asm__ volatile ("lfence" ::: "memory");
}

void smp_write_barrier(void) {
    __asm__ volatile ("sfence" ::: "memory");
}

/*
 * detect and initialize smp system
 */
int smp_init(void) {
    if (smp_initialized) {
        return 0;
    }
    
    LOG_INFO("smp", "initializing symmetric multiprocessing");
    
    /* initialize smp configuration */
    memset(&smp_config, 0, sizeof(smp_config));
    smp_config.local_apic_base = LOCAL_APIC_BASE;
    smp_config.io_apic_base = IO_APIC_BASE;
    
    /* detect cpus */
    if (detect_cpus_with_cpuid() != 0) {
        LOG_ERROR("smp", "failed to detect cpus");
        return -1;
    }
    
    /* initialize local apic for bootstrap processor */
    current_cpu_id = 0;
    if (smp_init_local_apic(current_cpu_id) != 0) {
        return -1;
    }
    
    /* initialize io apic */
    if (smp_init_io_apic() != 0) {
        return -1;
    }
    
    /* mark bootstrap processor as active */
    smp_config.cpus[0].cpu_id = 0;
    smp_config.cpus[0].apic_id = smp_get_current_cpu_apic_id();
    smp_config.cpus[0].local_apic_address = (void*)LOCAL_APIC_BASE;
    smp_config.cpus[0].flags = 0x01; /* active */
    smp_config.cpus[0].bsp = 1;
    
    /* set up timer for bootstrap processor */
    smp_setup_timer(0, 1000); /* 1khz timer */
    smp_start_timer(0);
    
    smp_initialized = 1;
    LOG_INFO("smp", "smp initialized successfully with %u cpus", smp_config.cpu_count);
    
    return 0;
}

/*
 * detect cpus
 */
int smp_detect_cpus(void) {
    return detect_cpus_with_cpuid();
}

/*
 * get cpu count
 */
uint8_t smp_get_cpu_count(void) {
    return smp_config.cpu_count;
}

/*
 * get cpu information
 */
cpu_info_t* smp_get_cpu_info(uint8_t cpu_id) {
    if (cpu_id >= smp_config.cpu_count) {
        return NULL;
    }
    
    return &smp_config.cpus[cpu_id];
}

/*
 * start additional cpu
 */
int smp_start_cpu(uint8_t cpu_id) {
    if (cpu_id >= smp_config.cpu_count || cpu_id == 0) {
        return -1;
    }
    
    /* check if cpu is already active */
    if (smp_cpu_is_active(cpu_id)) {
        return 0;
    }
    
    /* send ipi to start cpu */
    smp_send_ipi(cpu_id, 0x20); /* startup vector */
    
    /* wait for cpu to become active */
    for (int i = 0; i < 1000; i++) {
        if (smp_cpu_is_active(cpu_id)) {
            break;
        }
        /* delay */
    }
    
    if (smp_cpu_is_active(cpu_id)) {
        LOG_INFO("smp", "cpu %u started successfully", cpu_id);
        return 0;
    } else {
        LOG_WARNING("smp", "cpu %u failed to start", cpu_id);
        return -1;
    }
}

/*
 * stop cpu
 */
int smp_stop_cpu(uint8_t cpu_id) {
    if (cpu_id >= smp_config.cpu_count || cpu_id == 0) {
        return -1;
    }
    
    /* send ipi to stop cpu */
    smp_send_ipi(cpu_id, 0x21);
    
    smp_config.cpus[cpu_id].flags &= ~0x01;
    LOG_INFO("smp", "cpu %u stopped", cpu_id);
    
    return 0;
}

/*
 * put current cpu to sleep
 */
void smp_cpu_sleep(void) {
    __asm__ volatile ("hlt");
}

/*
 * wake specific cpu
 */
void smp_cpu_wake(uint8_t cpu_id) {
    /* send ipi to wake cpu */
    smp_send_ipi(cpu_id, 0x22);
}

/*
 * get cpu apic id
 */
uint8_t smp_get_cpu_apic_id(uint8_t cpu_id) {
    if (cpu_id >= smp_config.cpu_count) {
        return 0xff;
    }
    
    return smp_config.cpus[cpu_id].apic_id;
}

/*
 * unroute interrupt
 */
int smp_unroute_interrupt(uint8_t interrupt) {
    if (interrupt >= 24) {
        return -1;
    }
    
    /* mask interrupt */
    write_io_apic(0x10 + (interrupt * 2), 0x10000);
    
    return 0;
}

/*
 * print cpu information
 */
void smp_print_cpu_info(void) {
    LOG_INFO("smp", "cpu information:");
    LOG_INFO("smp", "  total cpus: %u", smp_config.cpu_count);
    LOG_INFO("smp", "  local apic base: 0x%x", smp_config.local_apic_base);
    LOG_INFO("smp", "  io apic base: 0x%x", smp_config.io_apic_base);
    
    for (uint8_t i = 0; i < smp_config.cpu_count; i++) {
        LOG_INFO("smp", "  cpu %u: apic_id %u, flags 0x%x%s", 
                 i, smp_config.cpus[i].apic_id, smp_config.cpus[i].flags,
                 (smp_config.cpus[i].flags & 0x01) ? " (active)" : "");
    }
}

/*
 * print interrupt routing
 */
void smp_print_interrupt_routing(void) {
    LOG_INFO("smp", "interrupt routing (first 16 interrupts):");
    for (int i = 0; i < 16; i++) {
        uint32_t redirection = read_io_apic(0x10 + (i * 2));
        uint8_t vector = (redirection >> 24) & 0xff;
        uint8_t apic_id = (redirection >> 12) & 0xff;
        uint8_t masked = (redirection & 0x10000) != 0;
        
        LOG_INFO("smp", "  int %u: vector 0x%02x, apic_id %u%s", 
                 i, vector, apic_id, masked ? " (masked)" : "");
    }
}