#ifndef MULTITASK_H
#define MULTITASK_H
#include <fs/vfs.h>
#include <memory/paging.h>
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
    VFS_Node *node;
    uint32_t offset;
    uint32_t flags;
    uint32_t refcount;
} OpenFile;

typedef struct {
    uintptr_t pd_phys;
    uintptr_t pt_phys;
    uint8_t thread_slot_bitmap[128];
    OpenFile *fds[16];
    uint32_t refcount;
} AddressSpace;

typedef struct TCB
{
    uint32_t task_id;
    uintptr_t saved_esp;
    TaskMain entry;
    AddressSpace *addr_space;
    uintptr_t kstack_low_phys;
    uintptr_t kstack_high_phys;
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

#define KSTACK_SIZE (8192)
#define KSTACK_PAGES (KSTACK_SIZE / BLOCK_SIZE)
#define KSTACK_TOP(slot)    ((uintptr_t)&HIGHER_HALF - (slot) * KSTACK_SIZE)
#define KSTACK_PTE(slot, p) (1023 - (slot) * KSTACK_PAGES - (p))  /* p=0 top page, p=1 lower page */
#define KERNEL_STACK_PDE_IDX (((uintptr_t)&HIGHER_HALF - KSTACK_SIZE) >> 22)

#define MAX_THREAD_SLOTS (1024 / KSTACK_PAGES)

#define USER_STACK_MAX_PAGES 256
#define USER_STACK_VADDR ((uintptr_t)&HIGHER_HALF - 0x400000)
#define user_region_top(slot) (USER_STACK_VADDR - (slot) * USER_STACK_MAX_PAGES * BLOCK_SIZE)
#define user_region_bottom(slot) (user_region_top(slot) - USER_STACK_MAX_PAGES * BLOCK_SIZE)

extern TCB *running;

void task_exit();

TCB *create_process_TCB(TaskMain function, bool supervisor);
bool create_process(TaskMain function, bool supervisor);
bool create_thread(TaskMain function, bool supervisor);
void block_running();
bool wake_task(uint32_t task_id);
void multitask_init();
#endif
