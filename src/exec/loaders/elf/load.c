// this loader wouldn't be possible without
//
//   - /usr/include/elf.h containing elf structures
//
//   - epsilon which was used as a reference implementation,
//     and ofc the person that archived it, because it would
//     be lost media otherwise
//     *  https://github.com/archivepedia/epsilon/tree/main/
//
//   - claude fixing my code lol (tho i need to refactor
//     some of its fixes) (also provided minimal elf
//     used in main.c (lol so fucking old comment, now
//     we can run GNU bash from initramfs))

#include <types.h>
#include <constants.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

#include "elf_internal.h"

elf_load_status_t elf_read_interp(const elf_image_t *image, const elf64_phdr_t *interp_ph,
                                  elf_load_result_t *result) {
    if (!elf_range_within(interp_ph->p_offset, interp_ph->p_filesz, image->size))
        return ELF_ERR_TRUNCATED;
    if (interp_ph->p_filesz >= ELF_INTERP_PATH_MAX)
        return ELF_ERR_INTERP_TOO_LONG;

    memcpy(result->interp_path, image->data + interp_ph->p_offset, interp_ph->p_filesz);
    result->interp_path[interp_ph->p_filesz] = '\0';
    result->has_interp = true;
    return ELF_OK;
}

elf_load_status_t elf_load_image(const uint8_t *data, uint64_t size, uint64_t *l4_table, uint64_t load_bias,
                                 const elf_object_t *external_modules, int external_count, bool apply_relocs,
                                 elf_load_result_t *result) {
    memset(result, 0, sizeof(*result));

    const elf64_ehdr_t *ehdr;
    elf_load_status_t status = elf_validate_header(data, size, &ehdr);
    if (status != ELF_OK) {
        result->status = status;
        return status;
    }

    if (ehdr->e_type == ET_EXEC) load_bias = 0;

    result->is_pie = ehdr->e_type == ET_DYN;
    result->load_bias = load_bias;
    result->phnum = ehdr->e_phnum;
    result->phentsize = ehdr->e_phentsize;

    elf_image_t image = {
        .data = data,
        .size = size,
        .ehdr = ehdr,
        .phdrs = (const elf64_phdr_t *)(data + ehdr->e_phoff),
        .phnum = ehdr->e_phnum,
        .phentsize = ehdr->e_phentsize,
    };

    const elf64_phdr_t *dynamic_ph = NULL;
    const elf64_phdr_t *interp_ph = NULL;

    for (uint16_t i = 0; i < image.phnum; i++) {
        const elf64_phdr_t *ph = elf_phdr_at(&image, i);

        if (ph->p_type == PT_DYNAMIC) {
            dynamic_ph = ph;
        } else if (ph->p_type == PT_INTERP) {
            interp_ph = ph;
        } else if (ph->p_type == PT_PHDR) {
            uint64_t phdr_vaddr;
            if (!__builtin_add_overflow(load_bias, ph->p_vaddr, &phdr_vaddr))
                result->phdr_vaddr = phdr_vaddr;
        }
    }

    status = elf_map_segments(&image, l4_table, load_bias, result);
    if (status != ELF_OK) {
        result->status = status;
        return status;
    }
    if (result->highest_vaddr == 0) {
        result->status = ELF_ERR_TRUNCATED;
        return ELF_ERR_TRUNCATED;
    }

    if (__builtin_add_overflow(load_bias, ehdr->e_entry, &result->entry)) {
        result->status = ELF_ERR_BAD_LAYOUT;
        return ELF_ERR_BAD_LAYOUT;
    }

    if (interp_ph) {
        status = elf_read_interp(&image, interp_ph, result);
        if (status != ELF_OK) {
            result->status = status;
            return status;
        }
    }

    if (dynamic_ph) {
        status = elf_process_dynamic(&image, l4_table, load_bias, dynamic_ph, external_modules,
                                     external_count, apply_relocs, result);
        if (status != ELF_OK) {
            result->status = status;
            return status;
        }
    }

    result->status = ELF_OK;
    return ELF_OK;
}
