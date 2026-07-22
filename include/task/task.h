#ifndef MULTITASK_H
#define MULTITASK_H
#include <stdbool.h>
#include <stdint.h>
typedef enum{

    READY = 0,
    RUNNING = 1,
    WAITING = 2,
    TERMINATED = 3

} TASK_STATE;

typedef void (*TaskMain)(void);

typedef struct TCB
{
    uint32_t task_id;
    uintptr_t saved_esp;
    uintptr_t stack_high; 
    uintptr_t stack_low; 
    TASK_STATE state;
    TaskMain entry;
    struct TCB* next;
} TCB;


bool create_task(TaskMain function);
void multitask_init();
#endif
