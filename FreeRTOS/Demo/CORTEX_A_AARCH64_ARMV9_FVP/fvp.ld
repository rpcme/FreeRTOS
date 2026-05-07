/*
 * Linker script for FVP_Base_RevC-2xAEMvA bare-metal FreeRTOS.
 * DRAM starts at 0x80000000. Stack at 0x84000000 (top of first 64MB).
 */

ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64M
}

SECTIONS
{
    .text : {
        *(.text.boot)
        *(.text*)
    } > RAM

    .rodata : ALIGN(8) {
        *(.rodata*)
    } > RAM

    .data : ALIGN(8) {
        *(.data*)
    } > RAM

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /DISCARD/ : {
        *(.comment)
        *(.note*)
    }
}
