#include <arch/i686/gdt.h>
#include <stdint.h>
typedef struct{
    uint16_t limit_low; // The lower 16 bits of the limit.
    uint16_t base_low;  // The lower 16 bits of the base.
    uint8_t base_middle; // The next 8 bits of the base.
    uint8_t access;      // Access flags, determine what ring this segment can be used in.
    uint8_t granularity; // Granularity and other flags.
    uint8_t base_high;   // The last 8 bits of the base.
}__attribute__((packed)) GDTEntry;

typedef struct{
    uint16_t limit; // sizeof(GDTEntry) - 1 .
    GDTEntry *ptr;  // The address of the first GDTEntry struct.
}__attribute__((packed)) GDTDescriptor;


typedef enum{
    GDT_ACCESS_CODE_READABLE = 0x02, // Code segment, readable, accessed
    GDT_ACCESS_DATA_WRITEABLE = 0x02, // Data segment, read/write

    GDT_ACCESS_CODE_CONFORMING = 0x04, // Code segment, conforming
    GDT_ACCESS_DATA_EXPAND_UP = 0x00, // Data segment, expand up
    GDT_ACCESS_DATA_EXPAND_DOWN = 0x04, // Data segment, expand down

    GDT_ACCESS_CODESEGMENT = 0x18, // Code segment
    GDT_ACCESS_DATASEGMENT = 0x10, // Data segment
    GDT_ACCESS_TSSSEGMENT = 0x00, // TSS segment

    GDT_ACCESS_RING0 = 0x00, // Ring 0
    GDT_ACCESS_RING1 = 0x20, // Ring 1
    GDT_ACCESS_RING2 = 0x40, // Ring 2
    GDT_ACCESS_RING3 = 0x60, // Ring 3

    GDT_ACCESS_PRESENT = 0x80, // Segment present, either enabled or disabled

} GDT_ACCESS;

typedef enum{
    GDT_FLAG_GRANULARITY_1B = 0x00, // Limit is in bytes
    GDT_FLAG_GRANULARITY_4KB = 0x80, // Limit is in 4KB pages

    GDT_FLAG_16BIT = 0x00, // 16-bit segment
    GDT_FLAG_32BIT = 0x40, // 32-bit segment
    GDT_FLAG_64BIT = 0x20, // 64-bit segment
} GDT_FLAG;

// Helper macro to create a GDT entry
#define GDT_ENTRY(base, limit, access, flags) \
    (GDTEntry){ \
        (uint16_t)((limit) & 0xFFFF), \
        (uint16_t)((base) & 0xFFFF), \
        (uint8_t)(((base) >> 16) & 0xFF), \
        (uint8_t)(access), \
        (uint8_t)(((limit) >> 16) & 0x0F) | ((flags) & 0xF0), \
        (uint8_t)(((base) >> 24) & 0xFF) \
    }


GDTEntry g_GDT[] = {
    GDT_ENTRY(0, 0, 0, 0),
    GDT_ENTRY(0, 0xFFFFF, 
        GDT_ACCESS_CODESEGMENT | GDT_ACCESS_CODE_READABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING0, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), //kernel Code segment
    GDT_ENTRY(0, 0xFFFFF, 
        GDT_ACCESS_DATASEGMENT | GDT_ACCESS_DATA_WRITEABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING0, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), //kernel Data segment
};

GDTDescriptor g_GDTDescriptor = {sizeof(g_GDT) - 1, g_GDT};

__attribute__((cdecl))
void i686_GDT_Load(GDTDescriptor *gdtDescriptor, uint16_t codeSegment, uint16_t dataSegment);


void i686_GDT_Init() {
    i686_GDT_Load(&g_GDTDescriptor, i686_GDT_CODE_SEGMENT, i686_GDT_DATA_SEGMENT);
}
