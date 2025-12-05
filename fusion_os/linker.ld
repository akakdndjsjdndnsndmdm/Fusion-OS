/*
 * linker.ld - fusion os linker script for x86-64
 * 
 * proper memory layout and section organization for fusion os kernel
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* sections */
SECTIONS
{
    /* multiboot header must be first */
    .multiboot 0x100000 :
    {
        *(.multiboot_header)
    }
    
    /* executable code */
    .text 0x100000 : ALIGN(4K)
    {
        *(.text)
    }
    
    /* read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }
    
    /* initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
    }
    
    /* bss - uninitialized data */
    .bss : ALIGN(4K)
    {
        *(.bss)
        *(COMMON)
    }
}