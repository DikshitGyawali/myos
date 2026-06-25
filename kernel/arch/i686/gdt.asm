[bits 32]

; void __attribute__((_cdecl)) i686_GDT_Load(GDTDescriptor *gdtDescriptor, uint16_t codeSegmentSelector, uint16_t dataSegmentSelector);
global i686_GDT_Load
i686_GDT_Load:

    push ebp
    mov ebp, esp

    ;load the GDT
    mov eax, [ebp + 8] ;gdtDescriptor
    lgdt [eax]

    
    ; reload segment registers
    ; reload code segment
    mov ax, [ebp + 12] ;codeSegmentSelector
    push eax
    push .reload_cs
    retf

.reload_cs:
    ; reload data segment
    mov ax, [ebp + 16] ;dataSegmentSelector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    mov esp, ebp
    pop ebp
ret
