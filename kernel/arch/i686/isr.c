#include <arch/i686/isr.h>
#include <arch/i686/idt.h>
#include <IO/screen.h>
#include <stddef.h>

ISRHandler g_ISRHandlers[256];


void i686_ISR_Initialize_Gates();

void i686_ISR_Init(){
    i686_ISR_Initialize_Gates();
    for (int i = 0; i < 255; ++i){
        i686_IDT_EnableGate(i);
    }
}


void __attribute__((cdecl)) i686_ISR_Handler(Registers* regs){

    if (g_ISRHandlers[regs->interrupt] != NULL){
        g_ISRHandlers[regs->interrupt](regs);
    }
    else if (regs->interrupt >= 32){
        kprintf("Interrupt: %x", regs->interrupt);

        kprintf("eax = %x,\tebx = %x,\tecx = %x,\nedx = %x,\tesi = %x,\tedi = %x\n", 
            regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
        
        kprintf("esp = %x, \tebp = %x, \teip = %x, \teflags = %x, \tcs = %x, \tds = %x, \tss = %x\n",
            regs->esp, regs->ebp, regs->eip, regs->eflags, regs->cs, regs->ds, regs->ss);

        kprintf("Errorcode: %x\n", regs->error);
    }
    else{
        kprintf("Exception: %x \n", regs->interrupt);

        kprintf("eax = %x,\tebx = %x,\tecx = %x,\nedx = %x,\tesi = %x,\tedi = %x\n", 
            regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
        
        kprintf("esp = %x, \tebp = %x, \teip = %x, \neflags = %x, \tcs = %x, \tds = %x, \tss = %x\n",
            regs->esp, regs->ebp, regs->eip, regs->eflags, regs->cs, regs->ds, regs->ss);

        kprintf("Errorcode: %x\n", regs->error);

        panic("Kernel Panic");
    }
}


void i686_ISR_RegisterHandler(int interrupt, ISRHandler handler){
    g_ISRHandlers[interrupt] = handler;
    i686_IDT_EnableGate(interrupt);
}

