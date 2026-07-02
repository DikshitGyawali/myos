#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <stdint.h>
#include <memory/boot_info.h>


void kernel_main(){
    HAL_Init();

    kprintf("Kernel real start: %x\n", (uint32_t)&_kernel_phys_start);
    kprintf("Kernel real end: %x\n", (uint32_t)&_kernel_phys_end);
    
    kprintf("Kernel virtual start: %x\n", (uint32_t)&_kernel_virt_start);
    kprintf("Kernel virtual end: %x\n", (uint32_t)&_kernel_virt_end);
    
    shell_Init();
    
    while (1) {
        __asm__("hlt");
    }
}
