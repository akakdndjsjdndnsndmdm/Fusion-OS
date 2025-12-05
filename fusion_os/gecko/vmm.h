/*
 * vmm.h - Virtual Memory Manager
 * 
 * Manages virtual address spaces using page tables and PMM.
 * Rejects impossible allocations like requesting 20GB when only 4GB available.
 */

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include "page_tables.h"
#include "pmm.h"

/* virtual memory flags */
#define VMM_READ    0x01
#define VMM_WRITE   0x02
#define VMM_EXEC    0x04
#define VMM_USER    0x08
#define VMM_KERNEL  0x10

/* gecko memory flags */
#define GECKO_MEMORY_READ    0x01
#define GECKO_MEMORY_WRITE   0x02
#define GECKO_MEMORY_EXEC    0x04
#define GECKO_MEMORY_USER    0x08

/* address space structure */
typedef struct {
    void* page_table_root;
    uint32_t flags;
    /* additional fields for process management */
} vmm_address_space_t;

/* vmm initialization */
void vmm_init(void);

/* address space management */
vmm_address_space_t* vmm_create_address_space(void);
void vmm_destroy_address_space(vmm_address_space_t* space);
void vmm_switch_address_space(vmm_address_space_t* space);

/* memory allocation with validation */
void* vmm_alloc_memory(vmm_address_space_t* space, size_t size, uint32_t flags);
void vmm_free_memory(vmm_address_space_t* space, void* addr, size_t size);

/* page-level operations */
void* vmm_alloc_page(vmm_address_space_t* space, uint32_t flags);
void vmm_free_page(vmm_address_space_t* space, void* addr);
int vmm_map_page(vmm_address_space_t* space, void* virtual_addr, 
                void* physical_addr, uint32_t flags);
void vmm_unmap_page(vmm_address_space_t* space, void* virtual_addr);

/* memory validation */
int vmm_is_memory_valid(void* addr, size_t size);
int vmm_can_allocate_memory(size_t requested_size);

/* kernel memory management */
void* vmm_alloc_kernel_memory(size_t size);
void vmm_free_kernel_memory(void* memory);

/* system information */
size_t vmm_get_total_virtual_memory(void);
size_t vmm_get_free_virtual_memory(void);

/* kernel address space access */
vmm_address_space_t* vmm_get_kernel_address_space(void);

#endif /* VMM_H */