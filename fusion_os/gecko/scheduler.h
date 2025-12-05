#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "../common/list.h"

/* task states */
#define TASK_RUNNING    0
#define TASK_READY      1
#define TASK_BLOCKED    2
#define TASK_SLEEPING   3
#define TASK_TERMINATED 4

/* scheduling policies */
#define SCHED_FIFO      0
#define SCHED_RR        1  /* round-robin */

/* task priorities */
#define PRIORITY_LOW    0
#define PRIORITY_NORMAL 1
#define PRIORITY_HIGH   2
#define PRIORITY_CRITICAL 3

/* maximum number of tasks */
#define MAX_TASKS 256

/* task control block */
typedef struct task_control_block {
    uint32_t task_id;
    char task_name[32];
    uint8_t state;
    uint8_t priority;
    uint8_t policy;
    uint32_t time_slice;
    uint32_t time_remaining;
    
    /* context information */
    void* kernel_stack;
    size_t stack_size;
    void* user_stack;
    void* page_table;
    
    /* timing information */
    uint64_t creation_time;
    uint64_t last_scheduled;
    uint64_t total_cpu_time;
    
    /* task linkage */
    list_node_t scheduler_list;
    list_node_t task_list;
    
    /* function pointer */
    void (*task_function)(void);
} task_t;

/* scheduler statistics */
typedef struct {
    uint32_t total_tasks;
    uint32_t running_tasks;
    uint32_t ready_tasks;
    uint32_t blocked_tasks;
    uint64_t total_schedules;
    uint64_t context_switches;
} scheduler_stats_t;

/* scheduler initialization */
void scheduler_init(void);
int scheduler_start(void);

/* task management */
int scheduler_create_task(void (*function)(void), const char* name, uint8_t priority);
int scheduler_create_thread(void* stack, size_t stack_size, void (*function)(void));
int scheduler_terminate_task(uint32_t task_id);
void scheduler_yield(void);
void scheduler_schedule(void);
void scheduler_sleep(uint32_t milliseconds);

/* scheduling control */
void scheduler_set_priority(uint32_t task_id, uint8_t priority);
uint8_t scheduler_get_priority(uint32_t task_id);
void scheduler_block_task(uint32_t reason);
void scheduler_unblock_task(uint32_t task_id);

/* system information */
task_t* scheduler_get_current_task(void);
uint32_t scheduler_get_task_count(void);
void scheduler_get_statistics(scheduler_stats_t* stats);

/* debugging */
void scheduler_print_task_list(void);
void scheduler_print_statistics(void);

#endif /* SCHEDULER_H */
