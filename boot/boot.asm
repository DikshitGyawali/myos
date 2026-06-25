BITS 16
ORG 0x7C00

jmp start
BOOT_DRIVE db 0
KERNEL_SIZE_TO_LOAD db 300 ; count of sector total bytes : count * 512 bytes

start:

	mov [BOOT_DRIVE], dl ; store the boot drive number passed by BIOS in DL for later use by the kernel
    call load_kernel
	cli ; disable interrupts before switching to protected mode
    call enable_a20
    call setup_gdt ; load the GDT with our code and data segment descriptors
    jmp switch_to_protected_mode 


load_kernel:
    mov ax, 0x0000
    mov es, ax
    mov bx, 0x1000 ; load the kernel at physical address 0x1000 (segment:offset(ES:BX) = 0x0000:0x1000)
    mov ah, 0x02
    mov al, [KERNEL_SIZE_TO_LOAD]
    mov ch, 0 
    mov cl, 2 ; sector 2 (after the bootloader, which is sector 1)
    mov dh, 0
	mov dl, [BOOT_DRIVE] ; use the boot drive number stored earlier
    int 0x13 ; call BIOS disk interrupt to read sectors into memory
    jc disk_error ; if carry flag is set, there was an error during disk read
    ret

disk_error:
	; Prints the error code on the screen for debugging purposes
    mov bl, ah
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
    mov al, bl
    add al, '0'
    int 0x10
    jmp halt

enable_a20:
    in  al, 0x92
    or  al, 0x02
    and al, 0xFE
    out 0x92, al
    ret

setup_gdt:
    lgdt [gdt_descriptor]
    ret

switch_to_protected_mode:
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:setup_stack    ; far jump, flushes the instruction pipeline and updates CS to CODE_SEG, which is the code segment descriptor in our GDT

; GDT lives here, outside any function
gdt_start:
gdt_null:   dq 0
gdt_code:   dw 0xffff, 0x0000
            db 0x00, 10011010b, 11001111b, 0x00
gdt_data:   dw 0xffff, 0x0000
            db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; size of GDT in bytes - 1
    dd gdt_start ; address of GDT

CODE_SEG equ gdt_code - gdt_start ; relative offset of code segment descriptor in GDT
DATA_SEG equ gdt_data - gdt_start ; relative offset of data segment descriptor in GDT

BITS 32
setup_stack:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000 ; set up the stack pointer to a safe location in memory (0x90000)
    mov eax, 0x1000 ; the physical address where the kernel is loaded
    jmp eax

halt:
    jmp halt

times 510 - ($ - $$) db 0
dw 0xAA55
