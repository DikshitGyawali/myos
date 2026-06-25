#include <memory/mmap.h>
#include <memory/boot_info.h>
#include <IO/screen.h>
#include <stdint.h>

void print_mmap_entries(uint32_t mbi_addr){
    MultibootInfo *mbi;
    mbi = (MultibootInfo*) mbi_addr;
    /* Are mmap_* valid? */
    kprintf("\n%x\n", mbi->mem_upper);
    kprintf("\n%x\n", mbi->mem_lower);
    if (CHECK_FLAG (mbi->flags, 6)){
        multiboot_mmap_entry *mmap;
        
        kprintf ("mmap_addr = %x, mmap_length = %x\n",
                (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
        for (mmap = (multiboot_mmap_entry *) mbi->mmap_addr;
            (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
            mmap = (multiboot_mmap_entry *) ((unsigned long) mmap
                                        + mmap->size + sizeof (mmap->size)))
            kprintf (" size = %x, base_addr = %x %x, length = %x %x, type = %x\n",
                    (unsigned) mmap->size,
                    (unsigned) (mmap->addr >> 32),
                    (unsigned) (mmap->addr & 0xffffffff),
                    (unsigned) (mmap->len >> 32),
                    (unsigned) (mmap->len & 0xffffffff),
                    (unsigned) mmap->type);
    }
}
