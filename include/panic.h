#ifndef PANIC_H
#define PANIC_H
#include <arch/i686/isr.h>
void panic(const char *expr, const char *file, int line);
void printReg(Registers *regs);

#endif
