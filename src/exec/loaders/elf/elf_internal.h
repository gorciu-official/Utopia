#pragma once

#include <types.h>
#include <mm/vmm.h>
#include <exec/loaders/elf.h>

#include "elf_structs.h"

#define ELF_USER_VA_LIMIT   (1ULL << USER_VA_BITS)
#define ELF_SYMBOL_NAME_MAX 512

typedef struct {
    uint64_t rela_vaddr;
    uint64_t rela_size;
    uint64_t rela_ent;
    uint64_t relr_vaddr;
    uint64_t relr_size;
    uint64_t relr_ent;
    uint64_t jmprel_vaddr;
    uint64_t jmprel_size;
    uint64_t pltrel_type;
    uint64_t symtab_vaddr;
    uint64_t strtab_vaddr;
    uint64_t strtab_size;
    uint64_t sym_count;
    uint64_t hash_vaddr;
    uint64_t gnu_hash_vaddr;
    bool has_legacy_rel;
} elf_dynamic_t;

static inline bool elf_range_within(uint64_t offset, uint64_t length, uint64_t limit) {
    return offset <= limit && length <= limit - offset;
}

static inline bool elf_user_range_ok(uint64_t address, uint64_t length) {
    return address < ELF_USER_VA_LIMIT && length <= ELF_USER_VA_LIMIT - address;
}

static inline bool elf_user_address(uint64_t base, uint64_t offset, uint64_t *out) {
    if (base >= ELF_USER_VA_LIMIT || offset >= ELF_USER_VA_LIMIT - base) return false;
    *out = base + offset;
    return true;
}

static inline const elf64_phdr_t *elf_phdr_at(const elf_image_t *image, uint16_t index) {
    return (const elf64_phdr_t *)((const uint8_t *)image->phdrs + (uint64_t)index * image->phentsize);
}

elf_load_status_t elf_validate_header(const uint8_t *data, uint64_t size, const elf64_ehdr_t **out_ehdr);

const void *elf_vaddr_to_fileptr(const elf_image_t *image, uint64_t vaddr, uint64_t size);
uint64_t elf_vaddr_file_span(const elf_image_t *image, uint64_t vaddr);

elf_load_status_t elf_map_segments(const elf_image_t *image, uint64_t *l4_table,
                                   uint64_t load_bias, elf_load_result_t *result);

elf_load_status_t elf_read_interp(const elf_image_t *image, const elf64_phdr_t *interp_ph,
                                  elf_load_result_t *result);

elf_load_status_t elf_process_dynamic(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                                      const elf64_phdr_t *dynamic_ph, const elf_object_t *external_modules,
                                      int external_count, bool apply_relocs, elf_load_result_t *result);

elf_load_status_t elf_load_image(const uint8_t *data, uint64_t size, uint64_t *l4_table, uint64_t load_bias,
                                 const elf_object_t *external_modules, int external_count, bool apply_relocs,
                                 elf_load_result_t *result);

int elf_load_full(const uint8_t *image, uint64_t image_size, uint64_t *l4_table,
                  elf_load_result_t *out_main, elf_auxv_info_t *out_auxv, uint64_t *out_entry);

int elf_apply_rela(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                   uint64_t table_vaddr, uint64_t table_size, uint64_t entry_size,
                   const elf_dynamic_t *dynamic, const elf_object_t *external_modules, int external_count);
int elf_apply_relr(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                   uint64_t table_vaddr, uint64_t table_size, uint64_t entry_size);

int elf_build_stack(const create_elf_process_request_t *request, const elf_auxv_info_t *auxv,
                    void *stack_base, size_t stack_size, uint64_t *out_sp);
