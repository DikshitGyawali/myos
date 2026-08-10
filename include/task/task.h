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

typedef struct {
    uintptr_t pd_phys;
    uintptr_t pt_phys;
    uint8_t thread_slot_bitmap[128];
    uint32_t refcount;
} AddressSpace;

typedef struct TCB
{
    uint32_t task_id;
    uintptr_t saved_esp;
    TaskMain entry;
    AddressSpace *addr_space;
    uintptr_t kstack_low_phys;
    uint32_t thread_slot;
    bool is_user;
    uint32_t ustack_committed_pages;
    TASK_STATE state;
    struct TCB* next;
} TCB;

static inline void set_thread_slot(AddressSpace *as, uint32_t slot){
    as->thread_slot_bitmap[slot / 8] |= (1 << (slot % 8));
}

static inline void clear_thread_slot(AddressSpace *as, uint32_t slot){
    as->thread_slot_bitmap[slot / 8] &= ~(1 << (slot % 8));
}

static inline bool test_thread_slot(AddressSpace *as, uint32_t slot){
    return (as->thread_slot_bitmap[slot / 8] & (1 << (slot % 8))) != 0;
}

#define KERNEL_STACK_VADDR ((uintptr_t)&HIGHER_HALF - BLOCK_SIZE)
#define KERNEL_STACK_PDE_IDX (KERNEL_STACK_VADDR >> 22)
#define USER_STACK_VADDR ((uintptr_t)&HIGHER_HALF - 0x400000)

#define USER_STACK_MAX_PAGES 256
#define user_region_top(slot) (USER_STACK_VADDR - slot * USER_STACK_MAX_PAGES * BLOCK_SIZE)
#define user_region_bottom(slot) (user_region_top(slot) - USER_STACK_MAX_PAGES * BLOCK_SIZE)


bool alloc_thread_slot(AddressSpace *as, uint32_t *out_slot);
void free_thread_slot(AddressSpace *as, uint32_t slot);

void task_exit();

bool create_process(TaskMain function, bool supervisor);
bool create_thread(TaskMain function, bool supervisor);
void multitask_init();
#endif
