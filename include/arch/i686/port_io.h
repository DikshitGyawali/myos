#ifndef IO_H
#define IO_H
#include <stdint.h>


static inline uint8_t i686_inb(uint16_t port){
    uint8_t ret;
    asm volatile ("inb %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

static inline void i686_outb(uint16_t port, uint8_t value){
    asm volatile ("outb %0, %1":/*no output*/: "a"(value), "Nd"(port));
}

static inline void i686_EnableInterrupts(){
    asm volatile ("sti");
}

static inline void i686_DisableInterrupts(){
    asm volatile ("cli");
}


void i686_iowait();

#endif // IO_H
