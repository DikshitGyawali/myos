bits 32

HIGHER_HALF    equ 0xC0000000
KERNEL_PDE_IDX equ (HIGHER_HALF >> 22)   ; = 768

MBOOT_MAGIC    equ 0x1BADB002
MBOOT_FLAGS    equ 0x00000003             ; 4K align + memory map
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

; ── Multiboot header 
section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

; ── Static page structures (VMA = LMA = physical address)
section .boot.data
align 4096
global boot_pd
boot_pd:        times 1024 dd 0

align 4096
boot_pt_low:    times 1024 dd 0   ; PDE   0: VA 0x00000000–0x3FFFFF → PA 0x0–0x3FFFFF

align 4096
boot_pt_high:   times 1024 dd 0   ; PDE 768: VA 0xC0000000–0xC3FFFFF → PA 0x0–0x3FFFFF

align 16
boot_stack_bottom:
    times 4096 db 0
boot_stack_top:

global saved_multiboot_ptr
saved_multiboot_ptr: dd 0

; ── Boot code (runs before paging, VMA = LMA = physical)
section .boot.text
extern kernel_high_entry

global _start
_start:
    cli
    mov  esp, boot_stack_top
    mov  [saved_multiboot_ptr], ebx     ; save before anything clobbers EBX

    ; Fill boot_pt_low: identity map first 4MB
    mov  edi, boot_pt_low
    mov  eax, 0x00000003                ; frame 0 | present | writable
.fill_low:
    mov  [edi], eax
    add  eax, 0x1000
    add  edi, 4
    cmp  edi, boot_pt_low + 1024 * 4
    jl   .fill_low

    ; Fill boot_pt_high: same physical frames, lives at PDE 768
    mov  edi, boot_pt_high
    mov  eax, 0x00000003
.fill_high:
    mov  [edi], eax
    add  eax, 0x1000
    add  edi, 4
    cmp  edi, boot_pt_high + 1024 * 4
    jl   .fill_high

    ; Wire both page tables and recursive slot into boot_pd
    mov  eax, boot_pt_low
    or   eax, 0x03
    mov  [boot_pd + 0 * 4], eax                ; PDE   0 → identity

    mov  eax, boot_pt_high
    or   eax, 0x03
    mov  [boot_pd + KERNEL_PDE_IDX * 4], eax   ; PDE 768 → kernel high

    mov  eax, boot_pd
    or   eax, 0x03
    mov  [boot_pd + 1023 * 4], eax             ; PDE 1023 → self (recursive)

    ; Load CR3 and enable paging
    mov  eax, boot_pd
    mov  cr3, eax

    mov  eax, cr0
    or   eax, 0x80000000
    mov  cr0, eax
    ; EIP is still at low address — PDE 0 identity mapping keeps the next fetch alive

    ; Switch ESP to its high-mapped alias before jumping
    ; same physical frame, now reached through PDE 768 instead of PDE 0
    add  esp, HIGHER_HALF

    ; Jump to high-linked entry point
    ; kernel_high_entry VMA ≈ 0xC010xxxx — linker encodes the correct rel32
    ; 32-bit modular arithmetic wraps correctly from low EIP to high target
    jmp  kernel_high_entry

    cli
    hlt
