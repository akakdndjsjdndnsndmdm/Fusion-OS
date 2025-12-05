/*
 * scheduler.c - Round-robin scheduler implementation
 * 
 * umm so based on OSDev research with proper task management and context switching.
 */

#include "scheduler.h"
#include "../common/logger.h"
#include "../common/string.h"
#include "vmm.h"

/* scheduler state */
static task_t tasks[MAX_TASKS];
static list_t ready_queue;
static list_t blocked_queue;
static list_t sleeping_queue;
static task_t* current_task = NULL;
static uint32_t next_task_id = 1;
static uint32_t task_count = 0;
static uint8_t scheduler_running = 0;

/* scheduling constants */
#define DEFAULT_TIME_SLICE 50      /* milliseconds */
#define MIN_TIME_SLICE 10          /* minimum time slice */
#define MAX_TIME_SLICE 1000        /* maximum time slice */

/* time tracking */
static uint64_t system_uptime = 0;
static uint64_t last_schedule_time = 0;

/* forward declarations */
void idle_task(void);

/*
 * find task by ID
 */
static task_t* find_task_by_id(uint32_t task_id) {
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].task_id == task_id && tasks[i].state != TASK_TERMINATED) {
            return &tasks[i];
        }
    }
    return NULL;
}

/*
 * select next task to run
 */
static task_t* select_next_task(void) {
    task_t* selected = NULL;
    
    /* check ready queue */
    list_node_t* node = list_get_head(&ready_queue);
    while (node != NULL) {
        task_t* task = (task_t*)node->data;
        if (task->state == TASK_READY) {
            selected = task;
            break;
        }
        node = node->next;
    }
    
    /* if no ready task, return current task if still valid */
    if (selected == NULL && current_task != NULL && current_task->state == TASK_RUNNING) {
        selected = current_task;
    }
    
    return selected;
}

/*
 * context switch assembly function
 * (would be implemented in assembly for performance)
 */
extern void context_switch(task_t* old_task, task_t* new_task);

/*
 * initialize scheduler
 */
void scheduler_init(void) {
    LOG_INFO("scheduler", "initializing scheduler");
    
    /* initialize task structures */
    memset(tasks, 0, sizeof(tasks));
    
    /* initialize queues */
    list_init(&ready_queue);
    list_init(&blocked_queue);
    list_init(&sleeping_queue);
    
    current_task = NULL;
    next_task_id = 1;
    task_count = 0;
    scheduler_running = 0;
    system_uptime = 0;
    last_schedule_time = 0;
    
    LOG_INFO("scheduler", "scheduler initialized");
}

/*
 * start scheduler
 */
int scheduler_start(void) {
    if (scheduler_running) {
        return 0;
    }
    
    /* create idle task */
    int idle_result = scheduler_create_task(idle_task, "idle", PRIORITY_LOW);
    if (idle_result < 0) {
        LOG_ERROR("scheduler", "failed to create idle task");
        return -1;
    }
    
    scheduler_running = 1;
    LOG_INFO("scheduler", "scheduler started");
    
    /* start first task */
    task_t* first_task = select_next_task();
    if (first_task != NULL) {
        current_task = first_task;
        current_task->state = TASK_RUNNING;
        LOG_INFO("scheduler", "starting first task: %s", current_task->task_name);
    }
    
    return 0;
}

/*
 * idle task function
 */
void idle_task(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/*
 * create new task
 */
int scheduler_create_task(void (*function)(void), const char* name, uint8_t priority) {
    if (task_count >= MAX_TASKS) {
        LOG_WARNING("scheduler", "maximum tasks reached");
        return -1;
    }
    
    /* find free task slot */
    uint32_t task_index = 0;
    while (task_index < MAX_TASKS && tasks[task_index].state != TASK_TERMINATED) {
        task_index++;
    }
    
    if (task_index >= MAX_TASKS) {
        return -1;
    }
    
    /* initialize task */
    task_t* task = &tasks[task_index];
    memset(task, 0, sizeof(task_t));
    
    task->task_id = next_task_id++;
    strncpy(task->task_name, name, sizeof(task->task_name) - 1);
    task->state = TASK_READY;
    task->priority = priority;
    task->policy = SCHED_RR;
    task->time_slice = DEFAULT_TIME_SLICE;
    task->time_remaining = DEFAULT_TIME_SLICE;
    task->task_function = function;
    task->creation_time = system_uptime;
    task->stack_size = 8192; /* 8KB stack */
    
    /* allocate kernel stack */
    task->kernel_stack = vmm_alloc_kernel_memory(task->stack_size);
    if (task->kernel_stack == NULL) {
        LOG_ERROR("scheduler", "failed to allocate stack for task %s", name);
        return -1;
    }
    
    /* initialize task list node */
    task->task_list.data = task;
    
    /* add to ready queue */
    list_add_tail(&ready_queue, &task->task_list);
    task_count++;
    
    LOG_INFO("scheduler", "created task %u: %s (priority %u)", 
             task->task_id, task->task_name, task->priority);
    
    return task->task_id;
}

/*
 * create thread with custom stack
 */
int scheduler_create_thread(void* stack, size_t stack_size, void (*function)(void)) {
    if (task_count >= MAX_TASKS) {
        return -1;
    }
    
    /* find free task slot */
    uint32_t task_index = 0;
    while (task_index < MAX_TASKS && tasks[task_index].state != TASK_TERMINATED) {
        task_index++;
    }
    
    if (task_index >= MAX_TASKS) {
        return -1;
    }
    
    /* initialize thread task */
    task_t* task = &tasks[task_index];
    memset(task, 0, sizeof(task_t));
    
    task->task_id = next_task_id++;
    strncpy(task->task_name, "thread", sizeof(task->task_name) - 1);
    task->state = TASK_READY;
    task->priority = PRIORITY_NORMAL;
    task->policy = SCHED_RR;
    task->time_slice = DEFAULT_TIME_SLICE;
    task->time_remaining = DEFAULT_TIME_SLICE;
    task->task_function = function;
    task->creation_time = system_uptime;
    task->kernel_stack = stack;
    task->stack_size = stack_size;
    
    task->task_list.data = task;
    list_add_tail(&ready_queue, &task->task_list);
    task_count++;
    
    return task->task_id;
}

/*
 * yield cpu to next task
 */
void scheduler_yield(void) {
    if (!scheduler_running || current_task == NULL) {
        return;
    }
    
    /* implement proper task yielding with time slice management */
    if (current_task->time_remaining > 0) {
        /* reduce time slice and reschedule if exhausted */
        current_task->time_remaining = 0;
        current_task->state = TASK_READY;
        
        /* move to end of ready queue for round-robin */
        if (current_task->policy == SCHED_RR) {
            list_remove(&ready_queue, &current_task->task_list);
            list_add_tail(&ready_queue, &current_task->task_list);
        }
        
        /* schedule next task */
        scheduler_schedule();
    }
}

/*
 * main scheduling function
 */
void scheduler_schedule(void) {
    if (!scheduler_running) {
        return;
    }
    
    task_t* old_task = current_task;
    task_t* new_task = select_next_task();
    
    if (new_task == NULL || new_task == old_task) {
        return; /* no task to switch to */
    }
    
    /* update timing information */
    uint64_t current_time = system_uptime;
    if (old_task != NULL && old_task->state == TASK_RUNNING) {
        old_task->total_cpu_time += current_time - old_task->last_scheduled;
        old_task->state = TASK_READY;
        
        /* move to end of ready queue if still ready */
        if (old_task->policy == SCHED_RR && old_task->time_remaining > 0) {
            list_remove(&ready_queue, &old_task->task_list);
            list_add_tail(&ready_queue, &old_task->task_list);
        }
    }
    
    /* set up new task */
    current_task = new_task;
    new_task->state = TASK_RUNNING;
    new_task->time_remaining = new_task->time_slice;
    new_task->last_scheduled = current_time;
    
    /* perform context switch */
    if (old_task != NULL) {
        context_switch(old_task, new_task);
    }
}

/*
 * terminate task
 */
int scheduler_terminate_task(uint32_t task_id) {
    task_t* task = find_task_by_id(task_id);
    if (task == NULL) {
        return -1;
    }
    
    task->state = TASK_TERMINATED;
    list_remove(&ready_queue, &task->task_list);
    list_remove(&blocked_queue, &task->task_list);
    list_remove(&sleeping_queue, &task->task_list);
    
    /* free resources */
    if (task->kernel_stack != NULL) {
        vmm_free_kernel_memory(task->kernel_stack);
    }
    
    task_count--;
    LOG_INFO("scheduler", "terminated task %u: %s", task_id, task->task_name);
    
    return 0;
}

/*
 * block current task
 */
void scheduler_block_task(uint32_t reason) {
    if (current_task == NULL) {
        return;
    }
    
    current_task->state = reason;
    list_remove(&ready_queue, &current_task->task_list);
    list_add_tail(&blocked_queue, &current_task->task_list);
    
    /* schedule next task */
    scheduler_schedule();
}

/*
 * unblock specific task
 */
void scheduler_unblock_task(uint32_t task_id) {
    task_t* task = find_task_by_id(task_id);
    if (task == NULL || task->state != TASK_BLOCKED) {
        return;
    }
    
    task->state = TASK_READY;
    list_remove(&blocked_queue, &task->task_list);
    list_add_tail(&ready_queue, &task->task_list);
    
    LOG_INFO("scheduler", "unblocked task %u: %s", task_id, task->task_name);
}

/*
 * set task priority
 */
void scheduler_set_priority(uint32_t task_id, uint8_t priority) {
    task_t* task = find_task_by_id(task_id);
    if (task != NULL) {
        task->priority = priority;
    }
}

/*
 * get task priority
 */
uint8_t scheduler_get_priority(uint32_t task_id) {
    task_t* task = find_task_by_id(task_id);
    return (task != NULL) ? task->priority : PRIORITY_LOW;
}

/*
 * get current task
 */
task_t* scheduler_get_current_task(void) {
    return current_task;
}

/*
 * get total task count
 */
uint32_t scheduler_get_task_count(void) {
    return task_count;
}

/*
 * print task list for debugging
 */
void scheduler_print_task_list(void) {
    LOG_INFO("scheduler", "task list:");
    
    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_TERMINATED) {
            LOG_INFO("scheduler", "  task %u: %s (state: %u, priority: %u)", 
                     tasks[i].task_id, tasks[i].task_name, 
                     tasks[i].state, tasks[i].priority);
        }
    }
}
