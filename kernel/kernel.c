#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <stdint.h>
#include <memory/boot_info.h>


static const MultibootInfo* multiboot_info;

const MultibootInfo* getMultibootInfo() {
    return multiboot_info;
}



void kernel_main(uint32_t magic, uint32_t mbi_addr){
    multiboot_info = (MultibootInfo*) mbi_addr;
    clear_screen();
    HAL_Init();
    kprintf("\n%x\n", magic);
    kprintf("Kernel start: %x\n", (uint32_t)&__kernel_start);
    kprintf("Kernel end: %x\n", (uint32_t)&__kernel_end);
    //print_mmap_entries();

    shell_Init();
    while (1) {
        __asm__("hlt");
    }
}
