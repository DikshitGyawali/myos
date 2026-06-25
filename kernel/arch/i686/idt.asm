[bits 32]

;void __attribute__((cdecl)) i686_IDT_Load(IDTDescriptor *idt_descriptor);
global i686_IDT_Load
i686_IDT_Load:

    push ebp
    mov ebp, esp

    mov eax, [ebp + 8] ; Load the pointer to the IDT descriptor into eax
    lidt [eax] ; Load the IDT using the lidt instruction

    mov esp, ebp
    pop ebp
    ret    
