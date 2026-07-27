/*
 * Minimal linker script for bare-metal AArch64.
 *
 * This is just a starting point. The load address (BASE) and overall layout
 * depend on your target:
 *   - QEMU "virt" machine : 0x40000000  (default below)
 *   - Raspberry Pi 3/4    : 0x00080000  (kernel8.img is loaded here)
 *
 * ENTRY(_start) names the first instruction to execute; your startup code
 * (e.g. src/start.S) must define a global `_start` label matching it.
 */

ENTRY(_start)

BASE = 0x40000000;

STACK_SIZE = 0x10000; /* 64 KIB */
NCPUS = 2;

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
}
