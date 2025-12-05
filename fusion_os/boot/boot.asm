/*
 * boot.asm - boot assembly and multiboot 2 header
 * 
 * boot code and multiboot header for fusion os on x86-64
 */

.code64

.section .multiboot_header, "aw"
    .align 8

    .long 0xe85250d6          /* multiboot 2 magic number */
    .long 0                   /* architecture: i386 */
    .long multiboot_header_end - multiboot_header_start
    .long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))
    
    .word 0                   /* type: end tag */
    .word 0                   /* flags */
    .long 8                   /* size */

multiboot_header_start:

.section .text
    .global _start
    .align 8

_start:
    /* setup initial stack */
    movq $stack_top, %rsp
    
    /* call main kernel entry point */
    call kernel_main
    
    /* if kernel_main returns, halt the system */
1:  hlt
    jmp 1b

.section .bss
    .align 16
stack_bottom:
    .skip 16384              /* 16kb stack size */
stack_top:

multiboot_header_end:
