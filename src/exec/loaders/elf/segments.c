#include <types.h>
#include <constants.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/string.h>

#include "elf_internal.h"

#define ELF_PAGE_SIZE 0x1000

static uint64_t segment_flags(uint32_t p_flags) {
    uint64_t flags = VMF_USER;
    if (p_flags & PF_W) flags |= VMF_WRITE;
    if (p_flags & PF_X) flags |= VMF_EXEC;
    return flags;
}

static bool find_file_backing(const elf_image_t *image, uint64_t vaddr, uint64_t *file_offset, uint64_t *available) {
    for (uint16_t i = 0; i < image->phnum; i++) {
        const elf64_phdr_t *ph = elf_phdr_at(image, i);
        if (ph->p_type != PT_LOAD) continue;
        if (vaddr < ph->p_vaddr) continue;

        uint64_t offset = vaddr - ph->p_vaddr;
        if (offset >= ph->p_filesz) continue;
        if (!elf_range_within(ph->p_offset, offset, image->size)) continue;

        *file_offset = ph->p_offset + offset;
        *available = ph->p_filesz - offset;
        return true;
    }
    return false;
}

const void *elf_vaddr_to_fileptr(const elf_image_t *image, uint64_t vaddr, uint64_t size) {
    uint64_t offset;
    uint64_t available;

    if (!find_file_backing(image, vaddr, &offset, &available)) return NULL;
    if (size > available) return NULL;
    if (!elf_range_within(offset, size, image->size)) return NULL;

    return image->data + offset;
}

uint64_t elf_vaddr_file_span(const elf_image_t *image, uint64_t vaddr) {
    uint64_t offset;
    uint64_t available;

    if (!find_file_backing(image, vaddr, &offset, &available)) return 0;
    if (offset > image->size) return 0;
    if (available > image->size - offset) return image->size - offset;
    return available;
}

static elf_load_status_t map_page(const elf_image_t *image, const elf64_phdr_t *ph,
                                  uint64_t *l4_table, uint64_t load_bias,
                                  uint64_t page_vaddr, uint64_t previous_flags) {
    uint8_t *destination = vm_user_to_kernel(l4_table, page_vaddr);
    uint64_t flags = segment_flags(ph->p_flags);

    if (destination) {
        if (protect_page_4k(l4_table, page_vaddr, flags | previous_flags) != 0)
            return ELF_ERR_MAP_FAILED;
    } else {
        void *page = pt_pool_alloc();
        if (!page) return ELF_ERR_NO_MEMORY;

        uint64_t phys = hhdm_virt_to_phys(page);
        if (!phys) return ELF_ERR_NO_MEMORY;

        destination = phys_to_virt(phys);
        memset(destination, 0, ELF_PAGE_SIZE);

        if (map_page_4k(l4_table, page_vaddr, phys, flags) != 0) {
            free(page);
            return ELF_ERR_MAP_FAILED;
        }
    }

    uint64_t file_start = load_bias + ph->p_vaddr;
    uint64_t file_end = file_start + ph->p_filesz;
    uint64_t page_end = page_vaddr + ELF_PAGE_SIZE;
    uint64_t copy_start = page_vaddr > file_start ? page_vaddr : file_start;
    uint64_t copy_end = page_end < file_end ? page_end : file_end;

    if (copy_end > copy_start) {
        uint64_t source = ph->p_offset + (copy_start - file_start);
        memcpy(destination + (copy_start - page_vaddr), image->data + source, copy_end - copy_start);
    }

    return ELF_OK;
}

static elf_load_status_t map_segment(const elf_image_t *image, const elf64_phdr_t *ph,
                                     uint64_t *l4_table, uint64_t load_bias,
                                     uint64_t *previous_flags, uint64_t *lowest, uint64_t *highest) {
    if (!elf_range_within(ph->p_offset, ph->p_filesz, image->size)) return ELF_ERR_TRUNCATED;
    if (ph->p_filesz > ph->p_memsz) return ELF_ERR_TRUNCATED;

    uint64_t segment_vaddr;
    if (__builtin_add_overflow(load_bias, ph->p_vaddr, &segment_vaddr)) return ELF_ERR_BAD_LAYOUT;
    if (!elf_user_range_ok(segment_vaddr, ph->p_memsz)) return ELF_ERR_BAD_LAYOUT;

    uint64_t segment_start = segment_vaddr & ~(ELF_PAGE_SIZE - 1);
    uint64_t segment_end = (segment_vaddr + ph->p_memsz + ELF_PAGE_SIZE - 1) & ~(ELF_PAGE_SIZE - 1);

    for (uint64_t page = segment_start; page < segment_end; page += ELF_PAGE_SIZE) {
        elf_load_status_t status = map_page(image, ph, l4_table, load_bias, page, *previous_flags);
        if (status != ELF_OK) return status;
    }

    *previous_flags = segment_flags(ph->p_flags);
    if (segment_start < *lowest) *lowest = segment_start;
    if (segment_end > *highest) *highest = segment_end;

    return ELF_OK;
}

elf_load_status_t elf_map_segments(const elf_image_t *image, uint64_t *l4_table,
                                   uint64_t load_bias, elf_load_result_t *result) {
    uint64_t previous_flags = 0;
    uint64_t lowest = ~0ULL;
    uint64_t highest = 0;

    for (uint16_t i = 0; i < image->phnum; i++) {
        const elf64_phdr_t *ph = elf_phdr_at(image, i);
        if (ph->p_type != PT_LOAD) continue;

        elf_load_status_t status = map_segment(image, ph, l4_table, load_bias,
                                               &previous_flags, &lowest, &highest);
        if (status != ELF_OK) return status;

        if (result->phdr_vaddr == 0 &&
            image->ehdr->e_phoff >= ph->p_offset &&
            image->ehdr->e_phoff - ph->p_offset < ph->p_filesz) {
            result->phdr_vaddr = load_bias + ph->p_vaddr + (image->ehdr->e_phoff - ph->p_offset);
        }
    }

    result->lowest_vaddr = lowest;
    result->highest_vaddr = highest;
    return ELF_OK;
}
