[bits 32]

global get_eflags ; uint32_t __attribute__((cdecl)) get_eflags();
get_eflags:
    pushfd
    pop eax
    ret


global get_cr2 ; uint32_t __attribute((cdecl)) get_cr2();
get_cr2:
    mov eax, cr2
    ret
