/*
 * context_switch.c - Context switching implementation for x86-64
 * 
 * This function switches between tasks by saving/restoring
 * CPU context including registers and stack pointers.
 */

#include "../common/string.h"
#include "scheduler.h"

/*
 * context_switch(old_task, new_task)
 * switches between tasks by saving/restoring CPU context
 */
void context_switch(task_t* old_task, task_t* new_task) {
    if (old_task == NULL || new_task == NULL) {
        return;
    }
    
    /* 
     * GCC inline assembly for context switching
     * This saves/restores all callee-saved registers and FP state
     */
    __asm__ __volatile__ (
        /* save old task context */
        "pushq %%rbp\n\t"
        "pushq %%rbx\n\t"
        "pushq %%r12\n\t"
        "pushq %%r13\n\t"
        "pushq %%r14\n\t"
        "pushq %%r15\n\t"
        
        /* save floating point state (simplified for now) */
        "subq $512, %%rsp\n\t"      /* space for FP state */
        "fxsave (%%rsp)\n\t"
        
        /* save current stack pointer */
        "movq %%rsp, (%0)\n\t"     /* old_task->kernel_stack = rsp */
        
        /* load new task context */
        "movq (%1), %%rsp\n\t"     /* rsp = new_task->kernel_stack */
        
        /* restore floating point state */
        "fxrstor (%%rsp)\n\t"
        "addq $512, %%rsp\n\t"
        
        /* restore new task context */
        "popq %%r15\n\t"
        "popq %%r14\n\t"
        "popq %%r13\n\t"
        "popq %%r12\n\t"
        "popq %%rbx\n\t"
        "popq %%rbp\n\t"
        
        /* return to new task */
        : /* no output operands */
        : "r" (old_task->kernel_stack), "r" (new_task->kernel_stack)
        : "memory", "cc"
    );
}
