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

static inline uint16_t i686_inw(uint16_t port){
    uint16_t ret;
    asm volatile ("inw %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

static inline void i686_outw(uint16_t port, uint16_t value){
    asm volatile ("outw %0, %1":/*no output*/: "a"(value), "Nd"(port));
}

static inline uint32_t i686_inl(uint16_t port){
    uint32_t ret;
    asm volatile ("inl %1, %0": "=a"(ret): "Nd"(port));
    return ret;
}

static inline void i686_outl(uint16_t port, uint32_t value){
    asm volatile ("outl %0, %1":/*no output*/: "a"(value), "Nd"(port));
}

static inline void i686_EnableInterrupts(){
    asm volatile ("sti");
}

static inline void i686_DisableInterrupts(){
    asm volatile ("cli");
}

#define UNUSED_PORT 0x80 
static inline void i686_iowait(){
    i686_outb(UNUSED_PORT, 0);
}

#endif // IO_H
