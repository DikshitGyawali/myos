#include <arch/i686/irq.h>
#include <drivers/pit.h>
#include <arch/i686/port_io.h>
#include <IO/screen.h>
#include <stdint.h>
#include <shell/shell.h>

#define PIT_CR 0x43
#define PIT_DATA_CHANNEL0 0x40

static volatile uint64_t g_ticks = 0;
static uint32_t frequency = 100;

typedef enum{
    PIT_COMMAND_CHANNEL0 = 0x00,
    PIT_COMMAND_CHANNEL1 = 0x01,
    PIT_COMMAND_CHANNEL2 = 0x02,
    PIT_COMMAND_READBACK = 0x03,

    PIT_COMMAND_LATCH = 0x00,
    PIT_COMMAND_LOWBYTE_ONLY = 0x10,
    PIT_COMMAND_HIGHBYTE_ONLY = 0x20,
    PIT_COMMAND_LOW_HIGH = 0x30,

    PIT_COMMAND_MODE0 = 0x00,
    PIT_COMMAND_MODE1 = 0x02,
    PIT_COMMAND_MODE2 = 0x04,
    PIT_COMMAND_MODE3 = 0x06,
    PIT_COMMAND_MODE4 = 0x08,
    PIT_COMMAND_MODE5 = 0x0A,

    PIT_COMMAND_BCD = 0x01,
    PIT_COMMAND_BINARY = 0x00
} PIT_COMMAND;


void PIT_Handler(Registers* regs)
{
    (void)regs;
    g_ticks++;
    //kprintf("%d ", g_ticks);
}

uint64_t PIT_GetTicks(){
    return g_ticks;
}
void CMD_GetTicks(char* args){
    (void)args;
    kprintf("%d\n", g_ticks);
}

uint32_t PIT_GetFrequency(){
    return frequency;
}

void PIT_Sleep(uint32_t ms)
{
    uint64_t start = g_ticks;
    uint64_t target = start + ms; // depends on tick rate

    while (g_ticks < target)
        __asm__("hlt");
}

void PIT_Init(uint32_t freq){
    frequency = freq;
    uint16_t divisor = 1193182 / frequency;
    i686_outb(PIT_CR, PIT_COMMAND_CHANNEL0 | 
                      PIT_COMMAND_LOW_HIGH |
                      PIT_COMMAND_MODE3 |
                      PIT_COMMAND_BINARY);
    
    i686_outb(PIT_DATA_CHANNEL0, divisor & 0xFF);
    i686_outb(PIT_DATA_CHANNEL0, divisor >> 8);
    i686_IRQ_RegisterHandler(0, PIT_Handler);
    shell_RegisterCommand((ShellCommand){"ticks", CMD_GetTicks});
}




