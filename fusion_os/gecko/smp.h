/*
 * smp.h - symmetric multiprocessing support for fusion os
 * 
 * implements smp initialization with local apic and io apic support
 * for multi-core x86-64 systems
 */

#ifndef SMP_H
#define SMP_H

#include <stdint.h>
#include <stddef.h>

/* apic definitions */
#define LOCAL_APIC_BASE      0xfee00000
#define LOCAL_APIC_SIZE      0x00001000

/* local apic register offsets */
#define LOCAL_APIC_ID        0x0020
#define LOCAL_APIC_VERSION   0x0030
#define LOCAL_APIC_TASK_PRIORITY 0x0080
#define LOCAL_APIC_ARBITRATION_PRIORITY 0x0090
#define LOCAL_APIC_PROCESSOR_PRIORITY 0x00a0
#define LOCAL_APIC_EOI       0x00b0
#define LOCAL_APIC_REMOTE_READ 0x00c0
#define LOCAL_APIC_LOGICAL_DEST 0x00d0
#define LOCAL_APIC_DEST_FORMAT  0x00e0
#define LOCAL_APIC_SPURIOUS_INTERRUPT_VECTOR 0x00f0
#define LOCAL_APIC_ISR_BASE     0x0100
#define LOCAL_APIC_TPR          0x0080
#define LOCAL_APIC_IRR_BASE     0x0200
#define LOCAL_APIC_ESR          0x0280
#define LOCAL_APIC_LVT_CMCI     0x02f0
#define LOCAL_APIC_TIMER        0x0320
#define LOCAL_APIC_THERMAL_SENSOR 0x0330
#define LOCAL_APIC_PERFORMANCE_COUNTER 0x0340
#define LOCAL_APIC_LINT0        0x0350
#define LOCAL_APIC_LINT1        0x0360
#define LOCAL_APIC_ERROR        0x0370

/* local apic timer */
#define LOCAL_APIC_TIMER_DIVIDE     0x03e0
#define LOCAL_APIC_TIMER_INITIAL_COUNT 0x0380
#define LOCAL_APIC_TIMER_CURRENT_COUNT 0x0390

/* io apic definitions */
#define IO_APIC_BASE           0xfec00000
#define IO_APIC_SIZE           0x00001000

/* io apic registers */
#define IO_APIC_VERSION        0x01
#define IO_APIC_ARBITRATION    0x02

/* interrupt routing entries */
#define MAX_INTERRUPTS         256
#define MAX_CPUS               64

/* cpu information */
typedef struct {
    uint8_t cpu_id;
    uint8_t apic_id;
    uint8_t socket_id;
    uint8_t flags;
    void* local_apic_address;
    uint32_t flags_local_apic;
    uint32_t bsp;
} cpu_info_t;

/* smp configuration */
typedef struct {
    uint8_t cpu_count;
    uint8_t io_apic_count;
    uint32_t local_apic_base;
    uint32_t io_apic_base;
    cpu_info_t cpus[MAX_CPUS];
} smp_config_t;

/* apic initialization */
int smp_init(void);
int smp_detect_cpus(void);
int smp_init_local_apic(uint8_t cpu_id);
int smp_init_io_apic(void);

/* cpu management */
uint8_t smp_get_cpu_count(void);
cpu_info_t* smp_get_cpu_info(uint8_t cpu_id);
int smp_start_cpu(uint8_t cpu_id);
int smp_stop_cpu(uint8_t cpu_id);

/* interrupt handling */
void smp_send_ipi(uint8_t target_cpu, uint8_t vector);
void smp_send_broadcast_ipi(uint8_t vector);
void smp_enable_interrupts(void);
void smp_disable_interrupts(void);

/* apic timer */
void smp_setup_timer(uint8_t cpu_id, uint32_t frequency_hz);
void smp_start_timer(uint8_t cpu_id);
void smp_stop_timer(uint8_t cpu_id);

/* cpu state management */
int smp_cpu_is_active(uint8_t cpu_id);
void smp_cpu_sleep(void);
void smp_cpu_wake(uint8_t cpu_id);

/* memory barriers */
void smp_memory_barrier(void);
void smp_read_barrier(void);
void smp_write_barrier(void);

/* cpu identification */
uint8_t smp_get_current_cpu_id(void);
uint8_t smp_get_current_cpu_apic_id(void);
uint8_t smp_get_cpu_apic_id(uint8_t cpu_id);

/* io apic functions */
int smp_route_interrupt(uint8_t interrupt, uint8_t cpu_id, uint8_t vector);
int smp_unroute_interrupt(uint8_t interrupt);

/* debugging */
void smp_print_cpu_info(void);
void smp_print_interrupt_routing(void);

#endif /* SMP_H */