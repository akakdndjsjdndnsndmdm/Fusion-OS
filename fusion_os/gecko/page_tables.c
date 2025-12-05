/*
 * page_tables.c - x86-64 page table implementation
 */

#include "page_tables.h"
#include "../common/logger.h"
#include <stddef.h>
#include <string.h>
#include "pmm.h"

/* walk page table to find PTE for virtual address */
pte_t* walk_page_table(void* page_table_root, void* virtual_addr) {
    uintptr_t addr = (uintptr_t)virtual_addr;
    pte_t* pt = (pte_t*)page_table_root;
    
    /* check if virtual address is in valid range */
    if (addr > 0x00007fffffffffffUL && addr < 0xffff800000000000UL) {
        return NULL;
    }
    
    /* traverse four-level page table */
    /* level 4: PML4 */
    pte_t pml4_entry = pt[PML4_INDEX(addr)];
    if (!pte_present(pml4_entry)) {
        return NULL;
    }
    pt = (pte_t*)pte_physical_address(pml4_entry);
    
    /* level 3: PDPT */
    pte_t pdpt_entry = pt[PDPT_INDEX(addr)];
    if (!pte_present(pdpt_entry)) {
        return NULL;
    }
    pt = (pte_t*)pte_physical_address(pdpt_entry);
    
    /* level 2: PD */
    pte_t pd_entry = pt[PD_INDEX(addr)];
    if (!pte_present(pd_entry)) {
        return NULL;
    }
    pt = (pte_t*)pte_physical_address(pd_entry);
    
    /* level 1: PT */
    return &pt[PT_INDEX(addr)];
}

/* map virtual address to physical address */
int map_virtual_address(void* page_table_root, void* virtual_addr, 
                       void* physical_addr, uint64_t flags) {
    uintptr_t vaddr = (uintptr_t)virtual_addr;
    pte_t* pt = (pte_t*)page_table_root;
    
    /* check virtual address range */
    if (vaddr > 0x00007fffffffffffUL && vaddr < 0xffff800000000000UL) {
        return -1;
    }
    
    /* level 4: PML4 */
    pte_t* pml4 = &pt[PML4_INDEX(vaddr)];
    if (!pte_present(*pml4)) {
        /* need to create PDPT */
        *pml4 = create_pte(create_page_table_page(), PTE_P | PTE_W);
    }
    
    /* level 3: PDPT */
    pte_t* pdpt = (pte_t*)pte_physical_address(*pml4);
    pte_t* pdpt_entry = &pdpt[PDPT_INDEX(vaddr)];
    if (!pte_present(*pdpt_entry)) {
        /* need to create PD */
        *pdpt_entry = create_pte(create_page_table_page(), PTE_P | PTE_W);
    }
    
    /* level 2: PD */
    pte_t* pd = (pte_t*)pte_physical_address(*pdpt_entry);
    pte_t* pd_entry = &pd[PD_INDEX(vaddr)];
    if (!pte_present(*pd_entry)) {
        /* need to create PT */
        *pd_entry = create_pte(create_page_table_page(), PTE_P | PTE_W);
    }
    
    /* level 1: PT */
    pte_t* ptable = (pte_t*)pte_physical_address(*pd_entry);
    pte_t* pte = &ptable[PT_INDEX(vaddr)];
    
    /* check if already mapped */
    if (pte_present(*pte)) {
        return -1;
    }
    
    *pte = create_pte(physical_addr, flags);
    return 0;
}

/* unmap virtual address */
void unmap_virtual_address(void* page_table_root, void* virtual_addr) {
    pte_t* pte = walk_page_table(page_table_root, virtual_addr);
    if (pte != NULL) {
        *pte = 0;
    }
}

/* get physical address for virtual address */
void* get_physical_address(void* page_table_root, void* virtual_addr) {
    pte_t* pte = walk_page_table(page_table_root, virtual_addr);
    if (pte != NULL && pte_present(*pte)) {
        return pte_physical_address(*pte) + PAGE_OFFSET((uintptr_t)virtual_addr);
    }
    return NULL;
}

/* initialize kernel page tables */
void init_kernel_page_tables(void) {
    LOG_INFO("page_tables", "initializing kernel page tables");
    /* this would be implemented with proper memory management */
}

/* create new page table page */
void* create_page_table_page(void) {
    void* page_table = pmm_alloc_page();
    if (page_table != NULL) {
        memset(page_table, 0, PAGE_SIZE);
    }
    return page_table;
}

/* destroy page table page */
void destroy_page_table_page(void* page_table) {
    if (page_table != NULL) {
        pmm_free_page(page_table);
    }
}

/* switch to different address space */
void switch_address_space(void* page_table_root) {
    __asm__ volatile ("movq %0, %%cr3" :: "r"(page_table_root));
}