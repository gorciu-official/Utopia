#include <types.h>
#include <constants.h>
#include <memory.h>
#include <types.h>
#include <arch/x86_64/common.h>
#include <process.h>
#include <lib/screen.h>

// this loader wouldn't be possible without
//
//   - /usr/include/elf.h containing elf structures
//
//   - epsilon which was used as a reference implementation,
//     and ofc the person that archived it, because it would 
//     be lost media otherwise
//      * https://github.com/archivepedia/epsilon/tree/main/
//
//   - claude fixing my code lol (tho i need to refactor
//     some of its fixes) (also provided minimal elf 
//     used in main.c)

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) Elf64_Phdr;

#define ELF_MAG0        0x7f
#define ELF_MAG1        'E'
#define ELF_MAG2        'L'
#define ELF_MAG3        'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EM_X86_64       0x3e

#define PT_NULL         0
#define PT_LOAD         1

#define PF_X            0x1
#define PF_W            0x2
#define PF_R            0x4

typedef enum {
    ELF_OK = 0,
    ELF_ERR_BAD_MAGIC,
    ELF_ERR_BAD_CLASS,
    ELF_ERR_BAD_ENDIAN,
    ELF_ERR_BAD_MACHINE,
    ELF_ERR_TRUNCATED,
    ELF_ERR_NO_MEMORY,
    ELF_ERR_MAP_FAILED,
} elf_load_status_t;

typedef struct {
    elf_load_status_t status;
    uint64_t entry;      
    uint64_t highest_vaddr; 
} elf_load_result_t;

static uint64_t page_flags_for(uint32_t p_flags) {
    uint64_t flags = 0;
    if (p_flags & PF_W) flags |= PAGE_RW;
    flags |= PAGE_USER;
    if (!(p_flags & PF_X)) flags |= PAGE_NX;
    return flags;
}

elf_load_result_t elf_load(const uint8_t* image, uint64_t image_size, uint64_t* l4_table) {
    elf_load_result_t result = {0};

    if (image_size < sizeof(elf64_ehdr)) {
        result.status = ELF_ERR_TRUNCATED;
        return result;
    }

    const elf64_ehdr* ehdr = (const elf64_ehdr*)image;

    if (ehdr->e_ident[0] != ELF_MAG0 || ehdr->e_ident[1] != ELF_MAG1 ||
        ehdr->e_ident[2] != ELF_MAG2 || ehdr->e_ident[3] != ELF_MAG3) {
        result.status = ELF_ERR_BAD_MAGIC;
        return result;
    }
    if (ehdr->e_ident[4] != ELFCLASS64) {
        result.status = ELF_ERR_BAD_CLASS;
        return result;
    }
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        result.status = ELF_ERR_BAD_ENDIAN;
        return result;
    }
    if (ehdr->e_machine != EM_X86_64) {
        result.status = ELF_ERR_BAD_MACHINE;
        return result;
    }

    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > image_size) {
        result.status = ELF_ERR_TRUNCATED;
        return result;
    }

    uint64_t highest = 0;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph = (const Elf64_Phdr*)(image + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);

        if (ph->p_type != PT_LOAD) continue;

        if (ph->p_offset + ph->p_filesz > image_size) {
            result.status = ELF_ERR_TRUNCATED;
            return result;
        }

        uint64_t seg_start = ph->p_vaddr & ~0xFFFULL;
        uint64_t seg_end   = (ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;
        uint64_t flags = page_flags_for(ph->p_flags);

        for (uint64_t page_va = seg_start; page_va < seg_end; page_va += 0x1000) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) {
                result.status = ELF_ERR_NO_MEMORY;
                return result;
            }

            uint8_t* dst = (uint8_t*)phys_to_virt(phys);
            for (int b = 0; b < 4096; b++) dst[b] = 0;

            uint64_t page_end = page_va + 0x1000;
            uint64_t file_lo = ph->p_vaddr;
            uint64_t file_hi = ph->p_vaddr + ph->p_filesz;

            uint64_t copy_lo = (page_va > file_lo) ? page_va : file_lo;
            uint64_t copy_hi = (page_end < file_hi) ? page_end : file_hi;

            if (copy_hi > copy_lo) {
                uint64_t src_off = ph->p_offset + (copy_lo - ph->p_vaddr);
                uint64_t dst_off = copy_lo - page_va;
                uint64_t len = copy_hi - copy_lo;
                for (uint64_t b = 0; b < len; b++) {
                    dst[dst_off + b] = image[src_off + b];
                }
            }

            if (map_page_4k(l4_table, page_va, phys, flags) != 0) {
                pmm_free_page(phys);
                result.status = ELF_ERR_MAP_FAILED;
                return result;
            }
        }

        if (seg_end > highest) highest = seg_end;
    }

    result.status = ELF_OK;
    result.entry = ehdr->e_entry;
    result.highest_vaddr = highest;
    return result;
}

int elf_start(const uint8_t* elf, uintptr_t size) {
    uint64_t* proc_l4 = clone_page_table();
    if (!proc_l4) return -1;

    elf_load_result_t res = elf_load(elf, size, proc_l4);
    if (res.status != ELF_OK) {
        free_page_table(proc_l4);
        return -1;
    }
    
    write_cr3(kernel_virt_to_phys(proc_l4)); // TODO: context switch should do this not me

    process_t* proc = process_create("jakis-elf", (void (*)(void*))res.entry, NULL, 3);
    if (!proc) {
        free_page_table(proc_l4);
        return -1;
    }

    proc->brk_start = (res.highest_vaddr + 0xFFFULL) & ~0xFFFULL;
    proc->brk_current = proc->brk_start;
    proc->page_table = proc_l4;

    return 0;
}
