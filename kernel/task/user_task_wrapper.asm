[bits 32]
global user_task_wrapper

section .user_wrapper
user_task_wrapper:
    call eax                    ; call entry() — ordinary ring-3 call, ret address pushed onto user stack

    mov eax, 1                 ; syscall number 1 = exit
    int 0x80                    ; trap back to ring 0 for cleanup — never returns
