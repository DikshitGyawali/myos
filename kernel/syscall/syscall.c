#include <syscall/syscall.h>
#include <arch/i686/isr.h>
#include <task/task.h>
#include <fs/syscalls.h>

void syscall_dispatch(Registers *regs){
    uint32_t syscall_num = regs->eax;
    switch (syscall_num)
    {
    case 1:
        task_exit();
        return;
    case 3:
        regs->eax = sys_read(regs->ebx, (void *)regs->ecx, regs->edx);
        return;
    case 4:
        regs->eax = sys_write(regs->ebx, (const void *)regs->ecx, regs->edx);
        return;
    default:
        break;
    }
}


void syscall_isr_init() {
    i686_ISR_RegisterHandler(0x80, syscall_dispatch);
}
