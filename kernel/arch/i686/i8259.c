#include <arch/i686/pic.h>
#include <arch/i686/port_io.h>
#include <stdint.h>
#include <stdbool.h>

#define PIC1_COMMAND_PORT 0x20
#define PIC1_DATA_PORT 0x21
#define PIC2_COMMAND_PORT 0xA0
#define PIC2_DATA_PORT 0xA1


enum{ // control word 1
    PIC_ICW1_ICW4 = 0x01,
    PIC_ICW1_SINGLE = 0x02,
    PIC_ICW1_INTERVAL4 = 0x04,
    PIC_ICW1_LEVEL = 0x08,
    PIC_ICW1_INITIALIZE = 0x10,
} PIC_ICW1;

enum{
    PIC_ICW4_8086 = 0x01,
    PIC_ICW4_AUTOEOI = 0x02,
    PIC_ICW4_BUFFER_MASTER = 0x04,
    PIC_ICW4_BUFFER_SLAVE = 0x00,
    PIC_ICW4_BUFFERRED = 0x08,
    PIC_ICW4_SFNM = 0x10

} PIC_ICW4;

enum{
    PIC_CMD_EOI = 0x20,
    PIC_CMD_READ_IRR = 0x0A,
    PIC_CMD_READ_ISR = 0x0B
} PIC_CMD;


static uint16_t g_PICMask = 0xFFFF;


void i8259_Disable();


void i8259_SetMask(uint16_t newMask){

    g_PICMask = newMask;
    i686_outb(PIC1_DATA_PORT, g_PICMask & 0xFF);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, g_PICMask >> 8);
    i686_iowait();

}


uint16_t i8259_GetMask(){
    return (((uint16_t)(i686_inb(PIC1_DATA_PORT) & 0xFF) | (uint16_t)(i686_inb(PIC2_DATA_PORT) << 8))); 
}


void i8259_Configure(uint8_t offsetPic1, uint8_t offsetPic2, bool autoEOI){

    // mask everything
    i8259_Disable();

    // initialize control word 1
    i686_outb(PIC1_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
    i686_iowait();
    i686_outb(PIC2_COMMAND_PORT, PIC_ICW1_ICW4 | PIC_ICW1_INITIALIZE);
    i686_iowait();

    // initialize control word 2 - offsets
    i686_outb(PIC1_DATA_PORT, offsetPic1);
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, offsetPic2);
    i686_iowait();

    // initialize control word 3
    i686_outb(PIC1_DATA_PORT, 0x04); // tell the master PIC, it has slave PIC connected through IRQ2(0000 0100)
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, 0x02); // tell the slave PIC, its cascade identity (0000 0010)IRQ2
    i686_iowait();

    // initialize control word 4

    uint8_t icw4 = PIC_ICW4_8086;
    if (autoEOI) icw4 |= PIC_ICW4_AUTOEOI;

    i686_outb(PIC1_DATA_PORT, PIC_ICW4_8086); 
    i686_iowait();
    i686_outb(PIC2_DATA_PORT, PIC_ICW4_8086);
    i686_iowait();

    // manually enable later
     i8259_Disable();
}


void i8259_Mask(int irq){
    i8259_SetMask(g_PICMask | ( 1 << irq));
}


void i8259_Unmask(int irq){
    i8259_SetMask(g_PICMask & ~( 1 << irq));
}


void i8259_Disable(){
    i8259_SetMask(0xFFFF);
}


void i8259_SendEOI(uint8_t irq){
    if (irq >= 8) i686_outb(PIC2_COMMAND_PORT, PIC_CMD_EOI);
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_EOI);
}


uint16_t i8259_ReadIQRRequestRegister(){
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_READ_IRR);
    i686_outb(PIC2_COMMAND_PORT, PIC_CMD_READ_IRR);
    return i686_inb(PIC1_COMMAND_PORT) | (i686_inb(PIC2_COMMAND_PORT) << 8);
}


uint16_t i8259_ReadInServiceRegister(){
    i686_outb(PIC1_COMMAND_PORT, PIC_CMD_READ_ISR);
    i686_outb(PIC2_COMMAND_PORT, PIC_CMD_READ_ISR);
    return i686_inb(PIC1_COMMAND_PORT) | (i686_inb(PIC2_COMMAND_PORT) << 8);
}

bool i8259_Probe(){
    i8259_SetMask(0x12ff);
    return (i8259_GetMask() == 0x12ff);
}


static const PICDriver g_PICDriver = {
    .Probe = &i8259_Probe,
    .Initialize = &i8259_Configure,
    .Disable = &i8259_Disable,
    .SendEOI = &i8259_SendEOI,
    .Mask = &i8259_Mask,
    .Unmask = &i8259_Unmask

};

const PICDriver* i8259_GetDriver(){
    return &g_PICDriver;
}
