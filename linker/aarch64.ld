/*
 * Minimal linker script for bare-metal AArch64.
 *
 * The load address (BASE) and overall layout depend on the target:
 *   - QEMU "virt" machine : 0x40080000
 *   - Raspberry Pi 3/4/5  : 0x00080000 (kernel8.img/kernel_2712.img is loaded here)
 *
 * ENTRY(_start) names the first instruction to execute. The startup code must
 * define a global label matching it.
 */

ENTRY(_start)

BASE = 0x40080000;

STACK_SIZE = 0x10000; /* 64 KIB */
NCPUS = 4;

SECTIONS
{
    . = BASE;

    .text : {
        KEEP(*(.text.boot))   /* put startup code first */
        *(.text)
        *(.text.*)
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    __stacks_bottom = .;
    . += STACK_SIZE * NCPUS;
    __stacks_top = .;
    __kernel_end = __stacks_top;
}
