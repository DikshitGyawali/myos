#include <IO/screen.h>
#include <arch/i686/isr.h>

void panic(const char *expr, const char *file, int line)
{
    kprintf("\n");
    kprintf("*** KERNEL PANIC ***\n");
    kprintf("Assertion failed: %s\n", expr);
    kprintf("File: %s\n", file);
    kprintf("Line: %d\n", line);

    asm volatile ("cli");

    for (;;) {
        asm volatile ("hlt");
    }
}

void printReg(Registers *regs){
    kprintf("Interrupt: %x\n", regs->interrupt);

    kprintf("eax = 0x%x,\tebx = 0x%x,\tecx = 0x%x,\nedx = 0x%x,\tesi = 0x%x,\tedi = 0x%x\n", 
        regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
    
    kprintf("esp = 0x%x, \tebp = 0x%x, \teip = 0x%x, \teflags = 0x%x, \tcs = 0x%x, \tds = 0x%x, \tss = 0x%x\n",
        regs->esp, regs->ebp, regs->eip, regs->eflags, regs->cs, regs->ds, regs->ss);

    kprintf("Errorcode: %x\n", regs->error);
}
