#include <hal/hal.h>
#include <IO/screen.h>
#include <arch/i686/gdt.h>
#include <arch/i686/idt.h>
#include <arch/i686/isr.h>
#include <arch/i686/irq.h>
#include <arch/i686/port_io.h>
#include <drivers/pit.h>
#include <drivers/keyboard.h>
#include <memory/pmm.h>
#include <memory/paging.h>

void Drivers_Init();

void HAL_Init(){
    kprintf("Initializing HAL...\n");
    kprintf("Initializing GDT...\n");
    i686_GDT_Init();
    kprintf("Initializing IDT...\n");
    i686_IDT_Init();
    kprintf("Initializing ISR...\n");
    i686_ISR_Init();
    kprintf("Initializing IRQ...\n");
    i686_IRQ_Init();
    kprintf("Initializing Drivers...\n");
    Drivers_Init();
    
    i686_EnableInterrupts();
}

void Drivers_Init(){
    PIT_Init(100);
    Keyboard_Init();
}

