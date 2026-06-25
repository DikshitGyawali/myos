#include <arch/i686/irq.h>
#include <arch/i686/pic.h>
#include <arch/i686/i8259.h>
#include <arch/i686/port_io.h>
#include <io/screen.h>
#include <stddef.h>
#define PIC_REMAP_OFFSET 0x20


IRQHandler g_IRQHandlers[16];
static const PICDriver* g_Driver = NULL;

void i686_IRQ_Handler(Registers* regs){
    int irq = regs->interrupt - PIC_REMAP_OFFSET;

    if (g_IRQHandlers[irq] != NULL){
        g_IRQHandlers[irq](regs);
    }
    else{
        kprintf("Unhandled Interrupt: %d\n", irq);
    }
    g_Driver->SendEOI(irq);
}


void i686_IRQ_Init(){

    const PICDriver* drivers[] = {
        i8259_GetDriver(),
        /*.......*/
    };
    for (size_t i = 0; i < sizeof(drivers)/ sizeof(PICDriver*); ++i){
        if (drivers[i]->Probe())
            g_Driver = drivers[i];
    }

    if (g_Driver == NULL){
        kprintf("Warning: No PIC Found\n");
    }

    g_Driver->Initialize(PIC_REMAP_OFFSET, PIC_REMAP_OFFSET + 8, false);

    for (int i = 0; i < 16; ++i){
        i686_ISR_RegisterHandler(PIC_REMAP_OFFSET + i, i686_IRQ_Handler);
    }

    
    g_Driver->Unmask(0); // timer
    g_Driver->Unmask(1); //keyboard
}


void i686_IRQ_RegisterHandler(int irq, IRQHandler handler){
    g_IRQHandlers[irq] = handler;   
}
