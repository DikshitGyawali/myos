#ifndef GDT_H
#define GDT_H
#include <stdint.h>

typedef struct{
    uint32_t link;
    uint32_t esp0;
    uint16_t ss0;
    uint16_t reserved_0;
    uint32_t esp1;
    uint16_t ss1;
    uint16_t reserved_1;
    uint32_t esp2;
    uint16_t ss2;
    uint16_t reserved_2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es;
    uint16_t reserved_3;
    uint16_t ss;
    uint16_t reserved_4;
    uint16_t ds;
    uint16_t reserved_5;
    uint16_t fs;
    uint16_t reserved_6;
    uint16_t gs;
    uint16_t reserved_7;
    uint16_t ldtr;
    uint16_t reserved_8;
    uint16_t reserved_9;
    uint16_t iopb;
}__attribute__((packed)) TSS;

extern TSS g_TSS;

void i686_GDT_init();
#define i686_GDT_CODE_SEGMENT 0x08
#define i686_GDT_DATA_SEGMENT 0x10
#define i686_GDT_USER_CODE_SEGMENT (0x18 | 0x03)
#define i686_GDT_USER_DATA_SEGMENT (0x20 | 0x03)
#define i686_GDT_TSS_SEGMENT 0x28

#endif // GDT_H
