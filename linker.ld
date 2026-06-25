ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);    /* R(4) + X(1) = 5 — for code */
    data PT_LOAD FLAGS(6);    /* R(4) + W(2) = 6 — for data */
}
SECTIONS
{
    . = 0x100000;
    __kernel_start = .;
    .multiboot  : {*(.multiboot*)}: text
    .text       : { *(.text*) }:text
    .rodata     : { *(.rodata*) } : text
    .data       : { *(.data*)   } :data
    .bss        : { *(COMMON) *(.bss*) } :data
    __kernel_end = .;
}
