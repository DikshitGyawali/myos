#include <syscall/syscall.h>
#include <arch/i686/isr.h>
#include <task/task.h>

void syscall_dispatch(Registers *regs){
    uint32_t syscall_num = regs->eax;
    switch (syscall_num)
    {
    case 0:
        task_exit();
        break;
    
    default:
        break;
    }
}


void syscall_isr_init() {
    i686_ISR_RegisterHandler(0x80, syscall_dispatch);
}
