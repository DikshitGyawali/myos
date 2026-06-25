#include <arch/i686/idt.h>
#include <stdint.h>
#include <util/binary.h>
typedef struct{
    uint16_t offset_low; // The lower 16 bits of the handler function's address.
    uint16_t selector;   // The code segment selector in GDT or LDT.
    uint8_t zero;        // This byte is set to zero.
    uint8_t flags;   // Type and attributes of the interrupt gate.
    uint16_t offset_high; // The upper 16 bits of the handler function's address.
}__attribute__((packed)) IDTEntry;

typedef struct{
    uint16_t limit;
    IDTEntry *ptr;
}__attribute__((packed)) IDTDescriptor;



IDTEntry g_IDT[256];
IDTDescriptor g_IDTDescriptor = {sizeof(g_IDT) - 1, g_IDT};



void __attribute__((cdecl)) i686_IDT_Load(IDTDescriptor *idt_descriptor);

void i686_IDT_SetGate(uint8_t interrupt_num, void* function_ptr, uint16_t segment_selector, uint8_t flags){
    g_IDT[interrupt_num].offset_low = ((uint32_t)function_ptr) & 0xFFFF;
    g_IDT[interrupt_num].selector = segment_selector;
    g_IDT[interrupt_num].zero = 0;
    g_IDT[interrupt_num].flags = flags;
    g_IDT[interrupt_num].offset_high = (((uint32_t)function_ptr) >> 16) & 0xFFFF;
}

void i686_IDT_EnableGate(int interrupt){
    FLAGSET(g_IDT[interrupt].flags, IDT_FLAG_PRESENT);
}

void i686_IDT_DisableGate(int interrupt){
    FLAGUNSET(g_IDT[interrupt].flags, IDT_FLAG_PRESENT);
}

void i686_IDT_Init(){
    i686_IDT_Load(&g_IDTDescriptor);
}
