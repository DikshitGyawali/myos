#ifndef IDT_H
#define IDT_H
#include <stdint.h>

typedef enum{
    IDT_FLAG_GATE_TASK = 0x5,
    IDT_FLAG_GATE_16BIT_INTERRUPT = 0x6,
    IDT_FLAG_GATE_16BIT_TRAP = 0x7,
    IDT_FLAG_GATE_32BIT_INTERRUPT = 0xE,
    IDT_FLAG_GATE_32BIT_TRAP = 0xF,

    IDT_FLAG_RING0 = (0 << 5),
    IDT_FLAG_RING1 = (1 << 5),
    IDT_FLAG_RING2 = (2 << 5),
    IDT_FLAG_RING3 = (3 << 5),

    IDT_FLAG_PRESENT = 0x80,


} IDT_FLAGS;


void i686_IDT_EnableGate(int interrupt);
void i686_IDT_DisableGate(int interrupt);
void i686_IDT_SetGate(uint8_t interrupt_num, void* function_ptr, uint16_t segment_selector, uint8_t flags);


void i686_IDT_Init();



#endif // IDT_H
