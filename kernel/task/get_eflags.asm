[bits 32]

global get_eflags
get_eflags:
    pushfd
    pop eax
    ret
