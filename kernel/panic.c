#include <IO/screen.h>


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
