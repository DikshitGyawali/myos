#include <memory/pmm.h>
#include <stdint.h>
#include <memory/boot_info.h>
#include <IO/screen.h>
#include <libs/memory.h>
#include <stdbool.h>
#include <stddef.h>

#define BLOCK_SIZE 4096
#define MAX_RESERVED_REGIONS 256

uint8_t *g_bitmap;
uint32_t g_bitmap_size;
uint32_t g_total_frames;

typedef struct
{
    uint32_t startFrame;
    uint32_t endFrame;
}Reserved_Frames;

Reserved_Frames g_reserved[MAX_RESERVED_REGIONS];
size_t g_reserved_counter = 0;

static inline void PMM_SetFrame(uint32_t frame){
    g_bitmap[frame / 8] |= (1<<(frame % 8));
}
static inline void PMM_ClearFrame(uint32_t frame){
    g_bitmap[frame / 8] &= ~(1<<(frame % 8));
}
static inline bool PMM_TestFrame(uint32_t frame){
    return (g_bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}


void PMM_PermanentReserveRange(uint32_t startFrame, uint32_t endFrame){
    for(uint32_t frame = startFrame; frame < endFrame; frame++){
        PMM_SetFrame(frame);
    }
    g_reserved[g_reserved_counter++] = (Reserved_Frames){startFrame, endFrame};
    return;
}

void Bitmap_Init(MultibootInfo const *mbi){
    uintptr_t bitmap_base = (uintptr_t)&__kernel_end;
    bitmap_base = (bitmap_base + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
    g_bitmap = (uint8_t *)bitmap_base;

    multiboot_mmap_entry *mmap;
    mmap = (multiboot_mmap_entry *) mbi->mmap_addr;
    uint64_t highest_address = 0;
    while((uintptr_t) mmap < (uintptr_t) mbi->mmap_addr + mbi->mmap_length){

        uint64_t end = mmap->addr + mmap->len;

        if(end > highest_address)
            highest_address = end;

        mmap = (multiboot_mmap_entry *) ((uintptr_t) mmap + mmap->size + sizeof (mmap->size));
    }

    kprintf("\nFinal Address: %x\n", highest_address - 1);

    g_total_frames = (highest_address + BLOCK_SIZE - 1)/ BLOCK_SIZE;
    g_bitmap_size = (g_total_frames + 7) / 8;
    kprintf("TotalFrames: %x\n", g_total_frames);
    kprintf("BitMapSize: %x\n", g_bitmap_size);

    memset(g_bitmap, 0xFF, g_bitmap_size);
}

void Bitmap_Setup(MultibootInfo const *mbi){
    multiboot_mmap_entry *mmap;
    mmap = (multiboot_mmap_entry *) mbi->mmap_addr;
    while((uintptr_t) mmap < (uintptr_t) mbi->mmap_addr + mbi->mmap_length){
        
        uint64_t start = (mmap->addr + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
        uint64_t end   = (mmap->addr + mmap->len) & ~(BLOCK_SIZE - 1);
        
        for(uint64_t addr = start; addr < end; addr += BLOCK_SIZE){

            uint32_t frame = (uint32_t)(addr / BLOCK_SIZE);

            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE){
                PMM_ClearFrame(frame); // free
            }
        }
        mmap = (multiboot_mmap_entry *) ((uintptr_t) mmap + mmap->size + sizeof (mmap->size));
    }
}

void Reserve_Kernel(){
    uint32_t start = (uintptr_t)&__kernel_start / BLOCK_SIZE;
    uint32_t end = ((uintptr_t)g_bitmap + g_bitmap_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    PMM_PermanentReserveRange(start, end);
}

void Reserve_MultibootInfo(MultibootInfo const *mbi){
    uint32_t start = (uintptr_t)mbi / BLOCK_SIZE;
    uint32_t end = ((uintptr_t)mbi + sizeof(MultibootInfo) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    PMM_PermanentReserveRange(start, end);

    start = (uintptr_t)mbi->mmap_addr/BLOCK_SIZE;
    end = ((uintptr_t)mbi->mmap_addr + mbi->mmap_length + BLOCK_SIZE - 1)/BLOCK_SIZE;
    PMM_PermanentReserveRange(start, end);
}

void PMM_Init(){
    MultibootInfo const *mbi;
    mbi = getMultibootInfo();
    if (!CHECK_FLAG (mbi->flags, 6)){
        return;
    }
    Bitmap_Init(mbi);
    Bitmap_Setup(mbi);
    Reserve_Kernel();
    Reserve_MultibootInfo(mbi);
}

static uint32_t frame_index = 0;

uintptr_t PMM_alloc_frame(){
    uint32_t first_frame = frame_index;
    while (frame_index < g_total_frames){
        if (!PMM_TestFrame(frame_index)){
            PMM_SetFrame(frame_index);
            frame_index++;
            return frame_index * BLOCK_SIZE;
        }
        frame_index++;
    }
    frame_index = 0;
    while (frame_index < first_frame){
        if (!PMM_TestFrame(frame_index)){
            PMM_SetFrame(frame_index);
            frame_index++;
            return (uintptr_t)frame_index * BLOCK_SIZE;
        }
        frame_index++;
    }
    return 0;
}

bool PMM_free_frame(uintptr_t addr){
    uint32_t frame = addr/BLOCK_SIZE;
    for (size_t i = 0; i < g_reserved_counter; ++i){
        if ((frame >= g_reserved[i].startFrame) && (frame < g_reserved[i].endFrame))
            return false;
    }

    PMM_ClearFrame(addr / BLOCK_SIZE);
    return true;
}

