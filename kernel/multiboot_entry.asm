[bits 32]
section .multiboot
dd 0x1BADB002
dd 0x00000003
dd -(MAGIC+FLAG)

extern kernel_main 
global _start
_start:

    mov esp, kernel_stack_top
    push ebx
    push eax
    call kernel_main
    cli
.halt
    hlt
    jmp .halt


MAGIC equ 0x1BADB002
FLAG equ 0x03

section .bss
align 16

kernel_stack:
    resb 16384

kernel_stack_top:
