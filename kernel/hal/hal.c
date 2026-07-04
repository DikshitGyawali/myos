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

void Drivers_init();

void HAL_init(){
    i686_GDT_init();
    i686_IDT_init();
    i686_ISR_init();
    i686_IRQ_init();
    Drivers_init();

    i686_EnableInterrupts();
}

void Drivers_init(){
    PIT_init(100);
    Keyboard_init();
}

