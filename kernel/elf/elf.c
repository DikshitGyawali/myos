#include <memory/boot_info.h>
#include <fs/vfs.h>
#include <memory/pmm.h>
#include <memory/paging.h>
#include <task/task.h>
#include <memory/heap.h>
#include <libs/mem_utils.h>
#include <error.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) Elf32_Phdr;


typedef enum {
	EI_MAG0		= 0, // 0x7F
	EI_MAG1		= 1, // 'E'
	EI_MAG2		= 2, // 'L'
	EI_MAG3		= 3, // 'F'
	EI_CLASS	= 4, // Architecture (32/64)
	EI_DATA		= 5, // Byte Order
	EI_VERSION	= 6, // ELF Version
	EI_OSABI	= 7, // OS Specific
	EI_ABIVERSION	= 8, // OS Specific
	EI_PAD		= 9  // Padding
} Elf_Ident;

# define ELFMAG0	    0x7F // e_ident[EI_MAG0]
# define ELFMAG1	    'E'  // e_ident[EI_MAG1]
# define ELFMAG2	    'L'  // e_ident[EI_MAG2]
# define ELFMAG3	    'F'  // e_ident[EI_MAG3]

# define ELFDATA2LSB    (1)  // Little Endian
# define ELFCLASS32	    (1)  // 32-bit Architecture



typedef enum {
	ET_NONE		= 0, // Unkown Type
	ET_REL		= 1, // Relocatable File
	ET_EXEC		= 2  // Executable File
} Elf_Type;

# define EM_386		(3)  // x86 Machine Type
# define EV_CURRENT	(1)  // ELF Current Version

#define PT_LOAD  1

#define PF_X  1
#define PF_W  2
#define PF_R  4

#define ELF_MAX_PHDRS        16
#define ELF_MAX_SEGMENT_SIZE (16u * 1024 * 1024)  // matches your disk image's own size

static int elf_load_headers(VFS_Node *elf_node, Elf32_Ehdr *out_ehdr, Elf32_Phdr *out_phdrs, uint32_t *out_phnum){
    int64_t r = elf_node->mount->driver->file_ops.read(elf_node, 0, out_ehdr, sizeof(Elf32_Ehdr));
    if (r < 0) return (int)r;
    if ((uint32_t)r < sizeof(Elf32_Ehdr)) return -ENOEXEC;

    if (out_ehdr->e_ident[EI_MAG0] != 0x7F || out_ehdr->e_ident[EI_MAG1] != 'E' ||
        out_ehdr->e_ident[EI_MAG2] != 'L'  || out_ehdr->e_ident[EI_MAG3] != 'F') return -ENOEXEC;
    if (out_ehdr->e_ident[EI_CLASS] != ELFCLASS32)  return -ENOEXEC;
    if (out_ehdr->e_ident[EI_DATA]  != ELFDATA2LSB) return -ENOEXEC;
    if (out_ehdr->e_type    != ET_EXEC) return -ENOEXEC;
    if (out_ehdr->e_machine != EM_386)  return -ENOEXEC;
    if (out_ehdr->e_phentsize != sizeof(Elf32_Phdr)) return -ENOEXEC;
    if (out_ehdr->e_phnum == 0 || out_ehdr->e_phnum > ELF_MAX_PHDRS) return -ENOEXEC;

    uint32_t table_bytes = out_ehdr->e_phnum * sizeof(Elf32_Phdr);
    r = elf_node->mount->driver->file_ops.read(elf_node, out_ehdr->e_phoff, out_phdrs, table_bytes);
    if (r < 0) return (int)r;
    if ((uint32_t)r < table_bytes) return -ENOEXEC;

    *out_phnum = out_ehdr->e_phnum;
    return 0;
}

static bool ranges_overlap(uintptr_t a_start, uintptr_t a_size, uintptr_t b_start, uintptr_t b_size){
    if (a_size == 0 || b_size == 0) return false;
    return a_start < b_start + b_size && b_start < a_start + a_size;
}

static int elf_validate_segments(Elf32_Phdr *phdrs, uint32_t phnum, uint64_t file_size, uint32_t thread_slot){
    for (uint32_t i = 0; i < phnum; i++){
        Elf32_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;
        if (phdrs[i].p_memsz == 0) continue;

        if (ph->p_memsz < ph->p_filesz) return -ENOEXEC;
        if (ph->p_memsz > ELF_MAX_SEGMENT_SIZE) return -ENOEXEC;

        if (ph->p_vaddr == 0) return -ENOEXEC;
        if (ph->p_vaddr >= (uintptr_t)&HIGHER_HALF) return -ENOEXEC;
        if (ph->p_memsz > (uintptr_t)&HIGHER_HALF - ph->p_vaddr) return -ENOEXEC;

        if (ph->p_filesz > file_size) return -ENOEXEC;
        if (ph->p_offset > file_size - ph->p_filesz) return -ENOEXEC;

        uintptr_t kstack_vaddr = (uintptr_t)&HIGHER_HALF - (thread_slot + 1) * BLOCK_SIZE;
        if (ranges_overlap(ph->p_vaddr, ph->p_memsz, kstack_vaddr, BLOCK_SIZE)) return -ENOEXEC;

        if (ranges_overlap(ph->p_vaddr, ph->p_memsz, user_region_bottom(thread_slot), USER_STACK_MAX_PAGES * BLOCK_SIZE)) return -ENOEXEC;

        for (uint32_t j = i + 1; j < phnum; j++){
            if (phdrs[j].p_type != PT_LOAD) continue;
            if (ranges_overlap(ph->p_vaddr, ph->p_memsz, phdrs[j].p_vaddr, phdrs[j].p_memsz)) return -ENOEXEC;
        }
    }
    return 0;
}

static bool install_page_in_pd(uint32_t *pd_virt, uintptr_t vaddr, uintptr_t phys, uint32_t flags){
    uint32_t pde_index = vaddr >> 22;
    uint32_t pte_index = (vaddr >> 12) & 0x3FF;

    if ((pd_virt[pde_index] & 0x1) == 0){
        uintptr_t pt_phys = PMM_alloc_frame();
        if (pt_phys == 0) return false;
        uint32_t *pt_virt = temp_map(pt_phys, 1);
        memset(pt_virt, 0, BLOCK_SIZE);
        temp_unmap(1);
        pd_virt[pde_index] = pt_phys | 0x07;  // PDE stays permissive — the real restriction lives at the PTE below
    }

    uint32_t *pt_virt = temp_map(pd_virt[pde_index] & ~0xFFF, 1);
    pt_virt[pte_index] = (phys & ~0xFFF) | flags;
    temp_unmap(1);
    return true;
}

static int map_segment(VFS_Node *elf_node, Elf32_Phdr *ph, uint32_t *pd_virt){
    uint32_t page_count = (ph->p_memsz + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uintptr_t base_vaddr = ph->p_vaddr & ~(BLOCK_SIZE - 1);

    uint32_t page_flags = 0x05;  // present + user, read-only by default
    if (ph->p_flags & PF_W) page_flags |= 0x02;

    for (uint32_t k = 0; k < page_count; k++){
        uintptr_t page_vaddr = base_vaddr + k * BLOCK_SIZE;

        uintptr_t phys = PMM_alloc_frame();
        if (phys == 0) return -ENOMEM;

        uint32_t *page_virt = temp_map(phys, 1);

        uint32_t file_bytes = 0;
        if (k * BLOCK_SIZE < ph->p_filesz){
            file_bytes = ph->p_filesz - k * BLOCK_SIZE;
            if (file_bytes > BLOCK_SIZE) file_bytes = BLOCK_SIZE;

            int64_t r = elf_node->mount->driver->file_ops.read(elf_node, ph->p_offset + k * BLOCK_SIZE, page_virt, file_bytes);
            if (r < 0 || (uint32_t)r < file_bytes){
                temp_unmap(1);
                PMM_free_frame(phys);
                return -ENOEXEC;
            }
        }
        memset((uint8_t *)page_virt + file_bytes, 0, BLOCK_SIZE - file_bytes);
        temp_unmap(1);

        if (!install_page_in_pd(pd_virt, page_vaddr, phys, page_flags)){
            PMM_free_frame(phys);
            return -ENOMEM;
        }
    }
    return 0;
}

void kill_task(TCB *task_to_kill);
void push_to_ready_queue(TCB *task);

TCB *elf_create_process(VFS_Node *elf_node, uint32_t entry, Elf32_Phdr *phdrs, uint32_t phnum, int *out_error){
    TCB *task = create_process_TCB((TaskMain)entry, false);
    if (task == NULL) return NULL;

    uint32_t *pd_virt = temp_map(task->addr_space->pd_phys, 0);

    for (uint32_t i = 0; i < phnum; i++){
        if (phdrs[i].p_type != PT_LOAD) continue;
        if (phdrs[i].p_memsz == 0) continue;

        int r = map_segment(elf_node, &phdrs[i], pd_virt);
        if (r < 0){
            temp_unmap(0);
            kill_task(task);
            *out_error = r;
            return NULL;
        }
    }

    temp_unmap(0);
    push_to_ready_queue(task);
    return task;
}

int elf_exec(const char *path){
    VFS_Node *elf_node = (VFS_Node *)kmalloc(sizeof(VFS_Node));
    if (elf_node == NULL) return -ENOMEM;

    int r = vfs_resolve_path(vfs_root, path, elf_node);
    if (r < 0){ kfree(elf_node); return r; }

    Elf32_Ehdr ehdr;
    Elf32_Phdr phdrs[ELF_MAX_PHDRS];
    uint32_t phnum;

    r = elf_load_headers(elf_node, &ehdr, phdrs, &phnum);
    if (r < 0){ kfree(elf_node); return r; }

    r = elf_validate_segments(phdrs, phnum, elf_node->size, 0);
    if (r < 0){ kfree(elf_node); return r; }

    TCB *task = elf_create_process(elf_node, ehdr.e_entry, phdrs, phnum, &r);
    kfree(elf_node);
    if (task == NULL) return r;
    return 0;
}
