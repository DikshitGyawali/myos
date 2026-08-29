OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

HIGHER_HALF = 0xC0000000;
KERNEL_PHYS = 0x00100000;

PHDRS
{
    boot_text PT_LOAD FLAGS(5);         /* R(4) + X(1):     .multiboot, .boot.text */
    boot_data PT_LOAD FLAGS(6);         /* R(4) + W(2):     .boot.data */
    text PT_LOAD FLAGS(5);              /* R(4) + X(1):         .text, .rodata */
    data PT_LOAD FLAGS(6);              /* R(4) + W(2):         .data, .bss */
    user_wrapper PT_LOAD FLAGS(5);      /* R(4) + X(1):         .user_wrapper */
}

SECTIONS
{
    HIGHER_HALF = 0xC0000000;
    . = KERNEL_PHYS;
    _kernel_phys_start = .;
    _kernel_virt_start = _kernel_phys_start + HIGHER_HALF;
    .boot ALIGN(4096) : {
        *(.multiboot)
        *(.boot.text)
    } :boot_text
    . = ALIGN(4096);
    .boot.data : {
        *(.boot.data)
    } :boot_data
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

    .user_wrapper ALIGN(4096) : AT(ADDR(.user_wrapper) - HIGHER_HALF){
        _user_wrapper_start = .;
        *(.user_wrapper)
    } :user_wrapper

    _kernel_virt_end = .;
    _kernel_phys_end = _kernel_virt_end - HIGHER_HALF;
}
