/* ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);   
    data PT_LOAD FLAGS(6);
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
} */

OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

HIGHER_HALF = 0xC0000000;
KERNEL_PHYS = 0x00100000;

PHDRS
{
    boot PT_LOAD FLAGS(7);   /* R(4) + W(2) + X(1): .boot section */
    text PT_LOAD FLAGS(5);   /* R(4) + X(1):         .text, .rodata */
    data PT_LOAD FLAGS(6);   /* R(4) + W(2):         .data, .bss */
}

SECTIONS
{
    . = KERNEL_PHYS;
    _kernel_phys_start = .;
    _kernel_virt_start = _kernel_phys_start + HIGHER_HALF;
    .boot ALIGN(4096) :{
        *(.multiboot)
        *(.boot.text)
        . = ALIGN(4096);    /* page-align the static page tables */
        *(.boot.data)
    } :boot
    _boot_end = .;
    . += HIGHER_HALF;
    
    .text ALIGN(4096) : AT(ADDR(.text) - HIGHER_HALF){ *(.text*) } :text
    .rodata ALIGN(4096) : AT(ADDR(.rodata) - HIGHER_HALF){ *(.rodata*) } :text
    .data ALIGN(4096) : AT(ADDR(.data) - HIGHER_HALF){ *(.data*) } :data
    .bss ALIGN(4096) : AT(ADDR(.bss) - HIGHER_HALF){
        _bss_start = .;
        *(COMMON)
        *(.bss*)
        _bss_end = .;
    } :data

    _kernel_virt_end = .;
    _kernel_phys_end = _kernel_virt_end - HIGHER_HALF;
}
