/*
 * page_tables.h - x86-64 page table structures and utilities
 * 
 * Implements the four-level page table system for x86-64 architecture
 * based on research from Harvard CS61 course materials.
 */

#ifndef PAGE_TABLES_H
#define PAGE_TABLES_H

#include <stdint.h>

/* page table entry flags */
#define PTE_P  (1 << 0)    /* present */
#define PTE_W  (1 << 1)    /* writable */
#define PTE_U  (1 << 2)    /* unprivileged (user accessible) */
#define PTE_PS (1 << 7)    /* page size (superpage) */
#define PTE_NX (1UL << 63) /* non-executable */

/* page table level constants */
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* address space layout for x86-64 */
#define KERNEL_VIRTUAL_BASE  0xffffffff80000000UL
#define USER_VIRTUAL_BASE    0x0000000000000000UL

/* page table indices */
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1ff)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1ff)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1ff)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1ff)
#define PAGE_OFFSET(addr) ((addr) & 0xfff)

/* page table entry structure */
typedef uint64_t pte_t;
typedef pte_t* page_table_t;

/*
 * page table entry manipulation functions
 */

/* check if page table entry is present */
static inline int pte_present(pte_t pte) {
    return (pte & PTE_P) != 0;
}

/* check if page table entry is writable */
static inline int pte_writable(pte_t pte) {
    return (pte & PTE_W) != 0;
}

/* check if page table entry is user accessible */
static inline int pte_user(pte_t pte) {
    return (pte & PTE_U) != 0;
}

/* check if page is large page (superpage) */
static inline int pte_large(pte_t pte) {
    return (pte & PTE_PS) != 0;
}

/* check if page is non-executable */
static inline int pte_nx(pte_t pte) {
    return (pte & PTE_NX) != 0;
}

/* get physical address from page table entry */
static inline void* pte_physical_address(pte_t pte) {
    return (void*)(pte & 0x000ffffffffff000UL);
}

/* create page table entry */
static inline pte_t create_pte(void* physical_addr, uint64_t flags) {
    return ((pte_t)physical_addr & 0x000ffffffffff000UL) | flags;
}

/*
 * page table traversal functions
 */

/* walk page table to find PTE for virtual address */
pte_t* walk_page_table(void* page_table_root, void* virtual_addr);

/* create new page table page */
void* create_page_table_page(void);

/* destroy page table page */
void destroy_page_table_page(void* page_table);

/* map virtual address to physical address */
int map_virtual_address(void* page_table_root, void* virtual_addr, 
                       void* physical_addr, uint64_t flags);

/* unmap virtual address */
void unmap_virtual_address(void* page_table_root, void* virtual_addr);

/* get physical address for virtual address */
void* get_physical_address(void* page_table_root, void* virtual_addr);

/* initialize kernel page tables */
void init_kernel_page_tables(void);

/* switch to different address space */
void switch_address_space(void* page_table_root);

#endif /* PAGE_TABLES_H */