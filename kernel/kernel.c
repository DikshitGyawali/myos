#include <IO/screen.h>
#include <hal/hal.h>
#include <shell/shell.h>
#include <stdint.h>
#include <memory/boot_info.h>
#include <memory/heap.h>


void kernel_main(){
    clear_screen();
    HAL_init();
    heap_init();

    shell_init();
    
    while (1) {
        __asm__("hlt");
    }
}
