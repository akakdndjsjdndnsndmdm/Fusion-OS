/*
 * pmm.c - Physical Memory Manager with buddy allocator implementation
 */

#include "pmm.h"
#include "../common/logger.h"
#include "../common/string.h"

/* global pmm instance */
static pmm_t global_pmm;
static int pmm_initialized = 0;

/* buddy allocator management structures */
#define MAX_MEMORY_REGIONS 32
static memory_map_entry_t memory_regions[MAX_MEMORY_REGIONS];
static uint32_t memory_region_count = 0;

/*
 * convert page order to page count
 */
static inline uint32_t pages_from_order(uint32_t order) {
    return 1 << order;
}

/*
 * convert page count to order
 */
uint32_t order_from_pages(uint32_t pages) {
    uint32_t order = 0;
    while ((1 << order) < pages) {
        order++;
    }
    return order;
}

/*
 * find the buddy of a memory block
 */
static inline memory_block_t* find_buddy(memory_block_t* block, uint32_t order) {
    uintptr_t block_addr = (uintptr_t)block;
    uintptr_t buddy_addr = block_addr ^ (pages_from_order(order) * PAGE_SIZE);
    return (memory_block_t*)buddy_addr;
}

/*
 * initialize the buddy allocator
 */
void pmm_init(void) {
    if (pmm_initialized) {
        return;
    }
    
    LOG_INFO("pmm", "initializing physical memory manager");
    
    /* initialize pmm structure */
    memset(&global_pmm, 0, sizeof(pmm_t));
    global_pmm.memory_start = 0;
    global_pmm.memory_end = 0;
    global_pmm.total_pages = 0;
    global_pmm.free_pages = 0;
    global_pmm.reserved_pages = 0;
    
    pmm_initialized = 1;
    LOG_INFO("pmm", "physical memory manager initialized");
}

/*
 * set memory map from bios memory detection
 */
void pmm_set_memory_map(memory_map_entry_t* map, uint32_t count) {
    if (!pmm_initialized) {
        pmm_init();
    }
    
    memory_region_count = 0;
    for (uint32_t i = 0; i < count && memory_region_count < MAX_MEMORY_REGIONS; i++) {
        if (map[i].type == MEMORY_AVAILABLE) {
            memory_regions[memory_region_count++] = map[i];
            global_pmm.total_pages += map[i].length / PAGE_SIZE;
            global_pmm.free_pages += map[i].length / PAGE_SIZE;
        }
    }
    
    global_pmm.memory_start = memory_regions[0].base;
    global_pmm.memory_end = memory_regions[memory_region_count - 1].base + 
                           memory_regions[memory_region_count - 1].length;
    
    LOG_INFO("pmm", "memory map set: %u regions, %u total pages", 
             memory_region_count, global_pmm.total_pages);
}

/*
 * allocate a single page
 */
void* pmm_alloc_page(void) {
    return pmm_alloc_pages(0);
}

/*
 * free a single page
 */
void pmm_free_page(void* page) {
    pmm_free_pages(page, 0);
}

/*
 * validate allocation request
 */
static int validate_allocation(uint32_t order) {
    uint32_t requested_pages = pages_from_order(order);
    uint32_t total_memory_gb = global_pmm.total_pages * PAGE_SIZE / (1024 * 1024 * 1024);
    uint32_t requested_gb = requested_pages * PAGE_SIZE / (1024 * 1024 * 1024);
    
    /* reject obviously impossible allocations */
    if (requested_gb > total_memory_gb && total_memory_gb > 0) {
        LOG_WARNING("pmm", "rejected allocation: %u gb requested, only %u gb available", 
                   requested_gb, total_memory_gb);
        return 0;
    }
    
    /* reject allocations larger than 50% of total system memory */
    if (requested_pages > global_pmm.total_pages / 2) {
        LOG_WARNING("pmm", "rejected large allocation: %u pages (> 50%% of %u total pages)", 
                   requested_pages, global_pmm.total_pages);
        return 0;
    }
    
    /* reject extremely large single allocations */
    if (requested_pages > (100 * 1024 * 1024 / PAGE_SIZE)) { /* 100 mb limit */
        LOG_WARNING("pmm", "rejected massive allocation: %u pages", requested_pages);
        return 0;
    }
    
    return 1;
}

/*
 * allocate pages of specified order
 */
void* pmm_alloc_pages(uint32_t order) {
    if (!pmm_initialized || order > MAX_ORDER) {
        return NULL;
    }
    
    /* validate allocation request */
    if (!validate_allocation(order)) {
        return NULL;
    }
    
    /* find smallest available order */
    uint32_t current_order = order;
    while (current_order <= MAX_ORDER && global_pmm.free_lists[current_order] == NULL) {
        current_order++;
    }
    
    if (current_order > MAX_ORDER) {
        LOG_WARNING("pmm", "out of memory at order %u", order);
        return NULL;
    }
    
    /* split from higher order if necessary */
    while (current_order > order) {
        memory_block_t* block = global_pmm.free_lists[current_order];
        global_pmm.free_lists[current_order] = block->next;
        
        /* split the block */
        uint32_t buddy_order = current_order - 1;
        memory_block_t* buddy = (memory_block_t*)((uint8_t*)block + 
                                                   (pages_from_order(buddy_order) * PAGE_SIZE));
        
        buddy->order = buddy_order;
        buddy->next = global_pmm.free_lists[buddy_order];
        global_pmm.free_lists[buddy_order] = buddy;
        
        current_order = buddy_order;
    }
    
    /* allocate the block */
    memory_block_t* block = global_pmm.free_lists[order];
    global_pmm.free_lists[order] = block->next;
    global_pmm.free_pages -= pages_from_order(order);
    
    return block;
}

/*
 * free pages of specified order
 */
void pmm_free_pages(void* pages, uint32_t order) {
    if (!pmm_initialized || order > MAX_ORDER || pages == NULL) {
        return;
    }
    
    memory_block_t* block = (memory_block_t*)pages;
    block->order = order;
    
    /* coalesce with buddies */
    while (order < MAX_ORDER) {
        memory_block_t* buddy = find_buddy(block, order);
        
        /* check if buddy is free and has same order */
        if (buddy->order != order) {
            break;
        }
        
        /* remove buddy from free list */
        memory_block_t** prev_ptr = &global_pmm.free_lists[order];
        while (*prev_ptr != buddy && *prev_ptr != NULL) {
            prev_ptr = &(*prev_ptr)->next;
        }
        
        if (*prev_ptr == buddy) {
            *prev_ptr = buddy->next;
            /* merge blocks */
            if ((uintptr_t)block > (uintptr_t)buddy) {
                block = buddy;
            }
            order++;
            block->order = order;
        } else {
            break;
        }
    }
    
    /* add to free list */
    block->next = global_pmm.free_lists[order];
    global_pmm.free_lists[order] = block;
    global_pmm.free_pages += pages_from_order(order);
}

/*
 * get total memory in bytes
 */
uint32_t pmm_get_total_memory(void) {
    return global_pmm.total_pages * PAGE_SIZE;
}

/*
 * get free memory in bytes
 */
uint32_t pmm_get_free_memory(void) {
    return global_pmm.free_pages * PAGE_SIZE;
}

/*
 * get used memory in bytes
 */
uint32_t pmm_get_used_memory(void) {
    return (global_pmm.total_pages - global_pmm.free_pages) * PAGE_SIZE;
}

/*
 * print memory statistics
 */
void pmm_print_statistics(void) {
    LOG_INFO("pmm", "memory statistics:");
    LOG_INFO("pmm", "  total: %u MB", pmm_get_total_memory() / (1024 * 1024));
    LOG_INFO("pmm", "  free: %u MB", pmm_get_free_memory() / (1024 * 1024));
    LOG_INFO("pmm", "  used: %u MB", pmm_get_used_memory() / (1024 * 1024));
}

/*
 * generic memory allocation
 */
void* pmm_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    /* round up to page size */
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t order = order_from_pages(pages_needed);
    
    return pmm_alloc_pages(order);
}

void pmm_free(void* ptr, size_t size) {
    if (ptr == NULL) {
        return;
    }
    
    /* round up to page size */
    size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t order = order_from_pages(pages_needed);
    
    pmm_free_pages(ptr, order);
}