OUTPUT_FORMAT(elf32-i386)
ENTRY(_user_start)


PHDRS
{
    text PT_LOAD FLAGS(5);              /* R(4) + X(1):         .text, .rodata */
    data PT_LOAD FLAGS(6);              /* R(4) + W(2):         .data, .bss */
}

SECTIONS
{
    .text : { *(.text*) } :text
    .rodata : { *(.rodata*) } :text
    .data : { *(.data*) } :data
    .bss : {
        *(COMMON)
        *(.bss*)
    } :data
}
