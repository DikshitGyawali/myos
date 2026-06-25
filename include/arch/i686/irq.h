#ifndef IRQ_H
#define IRQ_H

#include <arch/i686/isr.h>

typedef void (*IRQHandler)(Registers* regs);


void i686_IRQ_Init();
void i686_IRQ_RegisterHandler(int irq, IRQHandler handler);





#endif // IRQ_H
