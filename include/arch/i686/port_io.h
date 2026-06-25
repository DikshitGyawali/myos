#ifndef IO_H
#define IO_H
#include <stdint.h>


static inline uint8_t i686_inb(uint16_t port){
    uint8_t ret;
    asm volatile ("in %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

static inline void i686_outb(uint16_t port, uint8_t value){
    asm volatile ("out %1, %0": /*no output*/ :"Nd"(port), "a"(value));
}

static inline void i686_EnableInterrupts(){
    asm volatile ("sti");
}

static inline void i686_DisableInterrupts(){
    asm volatile ("cli");
}


void i686_iowait();

#endif // IO_H
