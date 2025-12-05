/*
 * pmm.h - Physical Memory Manager with buddy allocator
 * 
 * Implements a production-grade buddy allocator for managing physical memory.
 * Based on Linux kernel memory management research.
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

/* memory management constants */
#define MAX_ORDER 20           /* maximum allocation order (2^20 pages) */
#define PAGE_SIZE 4096         /* page size in bytes */
#define PAGE_SHIFT 12          /* page size shift amount */

/* memory region types */
#define MEMORY_AVAILABLE  0
#define MEMORY_RESERVED   1
#define MEMORY_ACPI       2
#define MEMORY_UNUSABLE   3

/* buddy allocator structures */
typedef struct memory_block {
    struct memory_block* next;
    uint32_t order;
    uint32_t flags;
} memory_block_t;

typedef struct {
    memory_block_t* free_lists[MAX_ORDER + 1];
    uintptr_t memory_start;
    uintptr_t memory_end;
    uint32_t total_pages;
    uint32_t free_pages;
    uint32_t reserved_pages;
} pmm_t;

/* memory map entry for BIOS memory detection */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} memory_map_entry_t;

/* pmm initialization */
void pmm_init(void);
void pmm_set_memory_map(memory_map_entry_t* map, uint32_t count);

/* page allocation and deallocation */
void* pmm_alloc_page(void);
void pmm_free_page(void* page);
void* pmm_alloc_pages(uint32_t order);
void pmm_free_pages(void* pages, uint32_t order);

/* memory statistics */
uint32_t pmm_get_total_memory(void);
uint32_t pmm_get_free_memory(void);
uint32_t pmm_get_used_memory(void);

/* memory information */
int pmm_memory_info(void* addr, uint32_t* flags);

/* debugging */
void pmm_print_statistics(void);

/* utility functions */
uint32_t order_from_pages(uint32_t pages);

/* generic memory allocation */
void* pmm_alloc(size_t size);
void pmm_free(void* ptr, size_t size);

#endif /* PMM_H */