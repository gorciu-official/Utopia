#include <types.h>
#include <mm/vmm.h>
#include <lib/string.h>

#include "elf_internal.h"

static bool read_symbol_name(const elf_image_t *image, uint64_t strtab_vaddr, uint64_t strtab_size,
                             uint32_t offset, char *buffer, uint64_t buffer_size) {
    if (!strtab_vaddr || offset >= strtab_size || buffer_size < 2) return false;
    if (__builtin_add_overflow(strtab_vaddr, (uint64_t)offset, &strtab_vaddr)) return false;

    uint64_t span = elf_vaddr_file_span(image, strtab_vaddr);
    if (span == 0) return false;

    const char *source = (const char *)elf_vaddr_to_fileptr(image, strtab_vaddr, 1);
    if (!source) return false;

    uint64_t remaining = strtab_size - offset;
    uint64_t limit = span < buffer_size ? span : buffer_size;
    if (limit > remaining) limit = remaining;

    for (uint64_t i = 0; i < limit; i++) {
        buffer[i] = source[i];
        if (buffer[i] == '\0') return true;
    }
    return false;
}

static bool read_module_symbol_name(const elf_object_t *module, uint32_t offset, char *buffer) {
    return read_symbol_name(&module->image, module->strtab_vaddr, module->strtab_size,
                            offset, buffer, ELF_SYMBOL_NAME_MAX);
}

static bool module_find_symbol(const elf_object_t *module, const char *name, uint64_t *out_value) {
    if (!module->symtab_vaddr || !module->strtab_vaddr || module->sym_count == 0) return false;

    for (uint64_t i = 1; i < module->sym_count; i++) {
        uint64_t symbol_vaddr;
        if (__builtin_add_overflow(module->symtab_vaddr, i * sizeof(elf64_sym_t), &symbol_vaddr)) break;

        const elf64_sym_t *symbol = (const elf64_sym_t *)elf_vaddr_to_fileptr(
            &module->image, symbol_vaddr, sizeof(elf64_sym_t)
        );
        if (!symbol) break;
        if (symbol->st_name == 0 || symbol->st_shndx == SHN_UNDEF) continue;

        char candidate[ELF_SYMBOL_NAME_MAX];
        if (!read_module_symbol_name(module, symbol->st_name, candidate)) continue;
        if (strcmp(candidate, name) != 0) continue;

        *out_value = module->load_bias + symbol->st_value;
        return true;
    }
    return false;
}

static bool resolve_external_symbol(const elf_object_t *modules, int module_count,
                                    const char *name, uint64_t *out_value) {
    for (int i = 0; i < module_count; i++) {
        if (module_find_symbol(&modules[i], name, out_value)) return true;
    }
    return false;
}

static const elf64_sym_t *symbol_at(const elf_image_t *image, const elf_dynamic_t *dynamic, uint64_t index) {
    if (!dynamic->symtab_vaddr) return NULL;
    if (dynamic->sym_count != 0 && index >= dynamic->sym_count) return NULL;

    uint64_t vaddr;
    if (__builtin_add_overflow(dynamic->symtab_vaddr, index * sizeof(elf64_sym_t), &vaddr)) return NULL;

    return (const elf64_sym_t *)elf_vaddr_to_fileptr(image, vaddr, sizeof(elf64_sym_t));
}

static bool resolve_rela_symbol(const elf_image_t *image, uint64_t load_bias, const elf_dynamic_t *dynamic,
                                const elf_object_t *external_modules, int external_count,
                                uint32_t type, uint64_t symbol_index, int64_t addend, uint64_t *out_value) {
    const elf64_sym_t *symbol = symbol_at(image, dynamic, symbol_index);
    if (!symbol) return false;

    uint64_t value;
    if (symbol->st_shndx == SHN_UNDEF) {
        char name[ELF_SYMBOL_NAME_MAX];
        if (!read_symbol_name(image, dynamic->strtab_vaddr, dynamic->strtab_size,
                              symbol->st_name, name, sizeof(name))) {
            return false;
        }
        if (!external_modules || external_count == 0) return false;
        if (!resolve_external_symbol(external_modules, external_count, name, &value)) return false;
    } else {
        value = load_bias + symbol->st_value;
    }

    if (type == R_X86_64_64) value += (uint64_t)addend;
    *out_value = value;
    return true;
}

int elf_apply_rela(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                   uint64_t table_vaddr, uint64_t table_size, uint64_t entry_size,
                   const elf_dynamic_t *dynamic, const elf_object_t *external_modules, int external_count) {
    if (entry_size == 0) entry_size = sizeof(elf64_rela_t);
    if (entry_size < sizeof(elf64_rela_t)) return -1;

    uint64_t count = table_size / entry_size;
    const uint8_t *table = (const uint8_t *)elf_vaddr_to_fileptr(image, table_vaddr, table_size);
    if (!table) return -1;

    for (uint64_t i = 0; i < count; i++) {
        const elf64_rela_t *rela = (const elf64_rela_t *)(table + i * entry_size);
        uint32_t type = ELF64_R_TYPE(rela->r_info);
        uint64_t symbol_index = ELF64_R_SYM(rela->r_info);

        uint64_t target_vaddr;
        if (!elf_user_address(load_bias, rela->r_offset, &target_vaddr)) return -2;

        void *target = vm_user_to_kernel(l4_table, target_vaddr);
        if (!target) return -2;

        if (type == R_X86_64_NONE) continue;

        if (type == R_X86_64_RELATIVE) {
            *(uint64_t *)target = load_bias + (uint64_t)rela->r_addend;
            continue;
        }

        if (type != R_X86_64_64 && type != R_X86_64_GLOB_DAT && type != R_X86_64_JUMP_SLOT) continue;

        uint64_t value;
        if (resolve_rela_symbol(image, load_bias, dynamic, external_modules, external_count,
                                type, symbol_index, rela->r_addend, &value)) {
            *(uint64_t *)target = value;
        }
    }

    return 0;
}

static bool relocate_relative(uint64_t *l4_table, uint64_t load_bias, uint64_t offset) {
    uint64_t vaddr;
    if (!elf_user_address(load_bias, offset, &vaddr)) return false;

    void *target = vm_user_to_kernel(l4_table, vaddr);
    if (!target) return false;

    *(uint64_t *)target = load_bias + *(uint64_t *)target;
    return true;
}

int elf_apply_relr(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                   uint64_t table_vaddr, uint64_t table_size, uint64_t entry_size) {
    if (entry_size == 0) entry_size = sizeof(uint64_t);
    if (entry_size != sizeof(uint64_t)) return -1;

    uint64_t count = table_size / entry_size;
    const uint64_t *entries = (const uint64_t *)elf_vaddr_to_fileptr(image, table_vaddr, table_size);
    if (!entries) return -1;

    uint64_t base = 0;
    for (uint64_t i = 0; i < count; i++) {
        uint64_t entry = entries[i];

        if ((entry & 1) == 0) {
            if (!relocate_relative(l4_table, load_bias, entry)) return -2;
            base = entry + sizeof(uint64_t);
            continue;
        }

        uint64_t bits = entry >> 1;
        uint64_t address = base;
        while (bits) {
            if (bits & 1) {
                if (!relocate_relative(l4_table, load_bias, address)) return -2;
            }
            bits >>= 1;
            address += sizeof(uint64_t);
        }
        base += 63 * sizeof(uint64_t);
    }

    return 0;
}
