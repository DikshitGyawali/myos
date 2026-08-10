[bits 32]

extern need_resched
extern running

extern i686_ISR_Handler
extern switch_task

%macro ISR_NOERRORCODE 1
global i686_ISR%1
i686_ISR%1:
    push 0; dummy error code
    push %1; push interrupt number
    jmp isr_common
%endmacro

%macro ISR_ERRORCODE 1
global i686_ISR%1
i686_ISR%1:
    ; cpu pushes the error code
    push %1 ; push interrupt number
    jmp isr_common
%endmacro

%include "arch/i686/isrs_cases.inc"


isr_common:
    cli
    pusha ; push edi, esi, ebp, esp, ebx, edx, ecx, eax

    xor eax, eax
    mov ax, ds
    push eax   ; push the current ds

    mov ax, 0x10 ; use kernel ds
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp ; pushing the stack pointer for C, so we can access all the register
    call i686_ISR_Handler
    add esp, 4 ; removes the stack pointer

    cmp byte [need_resched], 0
    jz restore
    ; context switching
    mov byte [need_resched], 0
    mov ebx, [running] ; old tcb
    mov [ebx + 4], esp ; old esp
    call switch_task

    mov ecx, [running] ; new tcb
    mov esp, [ecx + 4] ; new esp
    test eax, eax
    jz .no_reload
    mov cr3, eax
    .no_reload:


restore:
    pop eax ; get the previous ds, and restore the previous segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa ; pop what was pushed previously
    add esp,  8; remove the error code and interrupt number

    iret ; will pop cs, eip, eflags, ss, esp

