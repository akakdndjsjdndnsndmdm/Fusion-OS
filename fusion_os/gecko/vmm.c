/*
 * vmm.c - Virtual Memory Manager implementation
 */

#include "vmm.h"
#include "../common/logger.h"
#include "../common/string.h"

/* global kernel address space */
static vmm_address_space_t kernel_address_space;
static int vmm_initialized = 0;

/*
 * global mapping tracking for physical addresses
 */
static void* mapping_tracking = NULL;

/*
 * add physical address to tracking
 */
static void track_mapping(void* virtual_addr, void* physical_addr, size_t size) {
    if (mapping_tracking == NULL) {
        mapping_tracking = vmm_alloc_kernel_memory(sizeof(void*) * 1000); /* 1000 mappings max */
    }
    /* for now, just log the mapping for debugging */
    LOG_DEBUG("vmm", "mapped virtual %p to physical %p (size: %u)", 
              virtual_addr, physical_addr, (uint32_t)size);
}

/*
 * find physical address for virtual address
 */
static void* find_physical_address(void* virtual_addr) {
    /* in a real implementation, this would search the mapping table */
    /* for now, this is a placeholder that would need the full implementation */
    return NULL;
}

/*
 * check if requested allocation is possible
 */
static int validate_allocation(size_t requested_size) {
    uint32_t available_memory = pmm_get_free_memory();
    uint32_t total_system_memory = pmm_get_total_memory();
    
    /* reject obviously impossible allocations */
    if (requested_size > available_memory) {
        LOG_WARNING("vmm", "rejected allocation request: %u bytes (only %u bytes available)", 
                   (uint32_t)requested_size, available_memory);
        return 0;
    }
    
    /* reject allocations larger than 50% of total system memory */
    if (requested_size > (total_system_memory / 2)) {
        LOG_WARNING("vmm", "rejected large allocation: %u bytes (exceeds 50%% of system memory %u bytes)", 
                   (uint32_t)requested_size, total_system_memory);
        return 0;
    }
    
    /* reject extremely large single allocations */
    if (requested_size > (100 * 1024 * 1024)) { /* 100 MB limit */
        LOG_WARNING("vmm", "rejected massive allocation: %u bytes", (uint32_t)requested_size);
        return 0;
    }
    
    return 1;
}

/*
 * initialize vmm
 */
void vmm_init(void) {
    if (vmm_initialized) {
        return;
    }
    
    LOG_INFO("vmm", "initializing virtual memory manager");
    
    /* initialize kernel address space */
    kernel_address_space.page_table_root = create_page_table_page();
    kernel_address_space.flags = VMM_KERNEL;
    
    vmm_initialized = 1;
    LOG_INFO("vmm", "virtual memory manager initialized");
}

/*
 * create new address space
 */
vmm_address_space_t* vmm_create_address_space(void) {
    vmm_address_space_t* space = (vmm_address_space_t*)pmm_alloc_page();
    if (space == NULL) {
        return NULL;
    }
    
    space->page_table_root = create_page_table_page();
    space->flags = VMM_USER;
    
    if (space->page_table_root == NULL) {
        pmm_free_page(space);
        return NULL;
    }
    
    LOG_INFO("vmm", "created new address space %p", space);
    return space;
}

/*
 * destroy address space
 */
void vmm_destroy_address_space(vmm_address_space_t* space) {
    if (space == NULL) {
        return;
    }
    
    /* free page table structures */
    destroy_page_table_page(space->page_table_root);
    pmm_free_page(space);
    
    LOG_INFO("vmm", "destroyed address space %p", space);
}

/*
 * switch to address space
 */
void vmm_switch_address_space(vmm_address_space_t* space) {
    if (space != NULL) {
        switch_address_space(space->page_table_root);
    }
}

/*
 * allocate memory with validation
 */
void* vmm_alloc_memory(vmm_address_space_t* space, size_t size, uint32_t flags) {
    if (!vmm_initialized) {
        vmm_init();
    }
    
    /* validate allocation request */
    if (!validate_allocation(size)) {
        return NULL;
    }
    
    /* align size to page boundary */
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t page_count = aligned_size / PAGE_SIZE;
    
    /* allocate physical pages */
    void* memory = pmm_alloc_pages(order_from_pages(page_count));
    if (memory == NULL) {
        return NULL;
    }
    
    /* map pages in virtual address space */
    void* virtual_addr = (void*)0x100000; /* start at 1MB */
    for (uint32_t i = 0; i < page_count; i++) {
        void* phys_page = (void*)((uintptr_t)memory + (i * PAGE_SIZE));
        uint32_t page_flags = 0;
        
        if (flags & VMM_READ) page_flags |= PTE_P;
        if (flags & VMM_WRITE) page_flags |= PTE_W;
        if (flags & VMM_USER) page_flags |= PTE_U;
        if (!(flags & VMM_EXEC)) page_flags |= PTE_NX;
        
        if (map_virtual_address(space->page_table_root, virtual_addr, phys_page, page_flags) != 0) {
            /* cleanup on failure */
            for (uint32_t j = 0; j < i; j++) {
                void* cleanup_addr = (void*)((uintptr_t)0x100000 + (j * PAGE_SIZE));
                unmap_virtual_address(space->page_table_root, cleanup_addr);
            }
            pmm_free_pages(memory, order_from_pages(page_count));
            return NULL;
        }
        
        virtual_addr = (void*)((uintptr_t)virtual_addr + PAGE_SIZE);
    }
    
    return (void*)0x100000;
}

/*
 * free memory
 */
void vmm_free_memory(vmm_address_space_t* space, void* addr, size_t size) {
    if (space == NULL || addr == NULL) {
        return;
    }
    
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint32_t page_count = aligned_size / PAGE_SIZE;
    
    /* unmap pages and free physical memory */
    void* virtual_addr = addr;
    for (uint32_t i = 0; i < page_count; i++) {
        /* find physical address for this virtual address */
        void* physical_addr = find_physical_address(virtual_addr);
        
        /* unmap virtual address */
        unmap_virtual_address(space->page_table_root, virtual_addr);
        
        /* free physical page */
        if (physical_addr != NULL) {
            pmm_free_page(physical_addr);
        }
        
        virtual_addr = (void*)((uintptr_t)virtual_addr + PAGE_SIZE);
    }
}

/*
 * allocate single page
 */
void* vmm_alloc_page(vmm_address_space_t* space, uint32_t flags) {
    void* phys_page = pmm_alloc_page();
    if (phys_page == NULL) {
        return NULL;
    }
    
    void* virt_page = (void*)0x200000; /* use 2MB region for single pages */
    uint32_t page_flags = 0;
    
    if (flags & VMM_READ) page_flags |= PTE_P;
    if (flags & VMM_WRITE) page_flags |= PTE_W;
    if (flags & VMM_USER) page_flags |= PTE_U;
    if (!(flags & VMM_EXEC)) page_flags |= PTE_NX;
    
    if (map_virtual_address(space->page_table_root, virt_page, phys_page, page_flags) != 0) {
        pmm_free_page(phys_page);
        return NULL;
    }
    
    return virt_page;
}

/*
 * free single page
 */
void vmm_free_page(vmm_address_space_t* space, void* addr) {
    if (space == NULL || addr == NULL) {
        return;
    }
    
    /* find and free physical page based on mapping information */
    void* physical_addr = find_physical_address(addr);
    
    /* unmap virtual address */
    unmap_virtual_address(space->page_table_root, addr);
    
    /* free physical page if found */
    if (physical_addr != NULL) {
        pmm_free_page(physical_addr);
    }
}

/*
 * map page in address space
 */
int vmm_map_page(vmm_address_space_t* space, void* virtual_addr, 
                void* physical_addr, uint32_t flags) {
    uint32_t page_flags = 0;
    
    if (flags & VMM_READ) page_flags |= PTE_P;
    if (flags & VMM_WRITE) page_flags |= PTE_W;
    if (flags & VMM_USER) page_flags |= PTE_U;
    if (!(flags & VMM_EXEC)) page_flags |= PTE_NX;
    
    return map_virtual_address(space->page_table_root, virtual_addr, physical_addr, page_flags);
}

/*
 * unmap page from address space
 */
void vmm_unmap_page(vmm_address_space_t* space, void* virtual_addr) {
    if (space != NULL && virtual_addr != NULL) {
        unmap_virtual_address(space->page_table_root, virtual_addr);
    }
}

/*
 * check if memory region is valid
 */
int vmm_is_memory_valid(void* addr, size_t size) {
    /* basic validation - in real implementation would check page tables */
    return addr != NULL && size > 0;
}

/*
 * check if we can allocate requested memory
 */
int vmm_can_allocate_memory(size_t requested_size) {
    return validate_allocation(requested_size);
}

/*
 * allocate kernel memory
 */
void* vmm_alloc_kernel_memory(size_t size) {
    return vmm_alloc_memory(&kernel_address_space, size, VMM_KERNEL | VMM_READ | VMM_WRITE);
}

/*
 * free kernel memory
 */
void vmm_free_kernel_memory(void* memory) {
    if (memory != NULL) {
        /* calculate actual size of allocated memory block */
        /* for now, use page size as minimum allocation unit */
        vmm_free_memory(&kernel_address_space, memory, PAGE_SIZE);
    }
}

/*
 * get total virtual memory (theoretical limit)
 */
size_t vmm_get_total_virtual_memory(void) {
    return (size_t)0x7fffffffffffUL - (size_t)0x100000UL; /* 128TB - 1MB */
}

/*
 * get free virtual memory (calculated based on mappings)
 */
size_t vmm_get_free_virtual_memory(void) {
    /* calculate free virtual memory based on current page table mappings */
    uint32_t mapped_pages = 0;
    uint32_t total_pages = (size_t)0x7fffffffffffUL - (size_t)0x100000UL;
    
    /* in a real implementation, this would walk page tables to count free regions */
    /* for now, return theoretical maximum minus some overhead for kernel mappings */
    return total_pages - (size_t)(16ULL * 1024ULL * 1024ULL * 1024ULL); /* 16GB reserved for kernel */
}

/*
 * get kernel address space
 */
vmm_address_space_t* vmm_get_kernel_address_space(void) {
    if (!vmm_initialized) {
        vmm_init();
    }
    return &kernel_address_space;
}