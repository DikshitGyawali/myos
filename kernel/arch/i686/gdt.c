#include <arch/i686/gdt.h>
#include <stdint.h>
#include <libs/mem_utils.h>
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
    GDT_ACCESS_CODE_READABLE = 0x02, 
    GDT_ACCESS_DATA_WRITEABLE = 0x02, 

    GDT_ACCESS_CODE_CONFORMING = 0x04,
    GDT_ACCESS_DATA_EXPAND_UP = 0x00,
    GDT_ACCESS_DATA_EXPAND_DOWN = 0x04,

    GDT_ACCESS_CODESEGMENT = 0x18,
    GDT_ACCESS_DATASEGMENT = 0x10,
    GDT_ACCESS_TSSSEGMENT = 0x00,

    GDT_ACCESS_RING0 = 0x00,
    GDT_ACCESS_RING1 = 0x20,
    GDT_ACCESS_RING2 = 0x40,
    GDT_ACCESS_RING3 = 0x60,

    GDT_ACCESS_PRESENT = 0x80,

} GDT_ACCESS;

typedef enum{
    ACCESS_TYPE_16BIT_TSS_AVALIABLE = 0x01,
    ACCESS_TYPE_16BIT_TSS_BUSY = 0x03,

    ACCESS_TYPE_32BIT_TSS_AVALIABLE = 0x09,
    ACCESS_TYPE_32BIT_TSS_BUSY = 0x0B,

    ACCESS_LDT = 0x02,

} ACCESS_TYPE;

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
        GDT_ACCESS_CODESEGMENT | GDT_ACCESS_CODE_READABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING0, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), //Kernel Code Segment
    GDT_ENTRY(0, 0xFFFFF, 
        GDT_ACCESS_DATASEGMENT | GDT_ACCESS_DATA_WRITEABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING0, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), //Kernel Data Segment
    GDT_ENTRY(0, 0xFFFFF, 
        GDT_ACCESS_CODESEGMENT | GDT_ACCESS_CODE_READABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING3, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), // User Code Segmemt
    GDT_ENTRY(0, 0xFFFFF, 
        GDT_ACCESS_DATASEGMENT | GDT_ACCESS_DATA_WRITEABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING3, GDT_FLAG_GRANULARITY_4KB | GDT_FLAG_32BIT), //User Data Segment 
    GDT_ENTRY(0, 0, 0, 0), // place holder for TSS
};


static GDTDescriptor g_GDTDescriptor = {0, 0};

__attribute__((cdecl))
extern void i686_GDT_Load(GDTDescriptor *gdtDescriptor, uint16_t codeSegment, uint16_t dataSegment);
extern void __attribute__((cdecl)) i686_TSS_Load(uint16_t index);

TSS g_TSS;

void i686_TSS_init(){
    // fill TSS
    memset(&g_TSS, 0, sizeof(g_TSS));
    g_TSS.ss0 = i686_GDT_DATA_SEGMENT;
    g_TSS.iopb = sizeof(TSS);

    i686_TSS_Load(i686_GDT_TSS_SEGMENT);
}

void i686_GDT_init() {
    g_GDT[5] = GDT_ENTRY((uintptr_t)&g_TSS, sizeof(TSS) - 1, 
        GDT_ACCESS_TSSSEGMENT | ACCESS_TYPE_32BIT_TSS_AVALIABLE | GDT_ACCESS_PRESENT | GDT_ACCESS_RING0, GDT_FLAG_GRANULARITY_1B); //Task State Segment
    g_GDTDescriptor = (GDTDescriptor){sizeof(g_GDT) - 1, g_GDT};
    i686_GDT_Load(&g_GDTDescriptor, i686_GDT_CODE_SEGMENT, i686_GDT_DATA_SEGMENT);
    i686_TSS_init();
}
