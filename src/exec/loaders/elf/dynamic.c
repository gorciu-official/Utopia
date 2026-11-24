#include <types.h>

#include "elf_internal.h"

static void parse_dynamic_tags(const elf_image_t *image, const elf64_phdr_t *dynamic_ph,
                               elf_dynamic_t *dynamic) {
    const elf64_dyn_t *entries = (const elf64_dyn_t *)(image->data + dynamic_ph->p_offset);
    uint64_t count = dynamic_ph->p_filesz / sizeof(elf64_dyn_t);

    for (uint64_t i = 0; i < count; i++) {
        switch (entries[i].d_tag) {
        case DT_NULL:
            return;
        case DT_RELA:     dynamic->rela_vaddr = entries[i].d_un.d_ptr; break;
        case DT_RELASZ:   dynamic->rela_size = entries[i].d_un.d_val; break;
        case DT_RELAENT:  dynamic->rela_ent = entries[i].d_un.d_val; break;
        case DT_SYMTAB:   dynamic->symtab_vaddr = entries[i].d_un.d_ptr; break;
        case DT_STRTAB:   dynamic->strtab_vaddr = entries[i].d_un.d_ptr; break;
        case DT_STRSZ:    dynamic->strtab_size = entries[i].d_un.d_val; break;
        case DT_HASH:     dynamic->hash_vaddr = entries[i].d_un.d_ptr; break;
        case DT_GNU_HASH: dynamic->gnu_hash_vaddr = entries[i].d_un.d_ptr; break;
        case DT_JMPREL:   dynamic->jmprel_vaddr = entries[i].d_un.d_ptr; break;
        case DT_PLTRELSZ: dynamic->jmprel_size = entries[i].d_un.d_val; break;
        case DT_PLTREL:   dynamic->pltrel_type = entries[i].d_un.d_val; break;
        case DT_REL:      dynamic->has_legacy_rel = true; break;
        case DT_RELR:     dynamic->relr_vaddr = entries[i].d_un.d_ptr; break;
        case DT_RELRSZ:   dynamic->relr_size = entries[i].d_un.d_val; break;
        case DT_RELRENT:  dynamic->relr_ent = entries[i].d_un.d_val; break;
        default: break;
        }
    }
}

static uint64_t sysv_hash_symbol_count(const elf_image_t *image, uint64_t hash_vaddr) {
    const uint32_t *header = (const uint32_t *)elf_vaddr_to_fileptr(image, hash_vaddr, 2 * sizeof(uint32_t));
    return header ? header[1] : 0;
}

static uint64_t gnu_hash_symbol_count(const elf_image_t *image, uint64_t gnu_hash_vaddr) {
    const uint32_t *header = (const uint32_t *)elf_vaddr_to_fileptr(image, gnu_hash_vaddr, 4 * sizeof(uint32_t));
    if (!header) return 0;

    uint32_t bucket_count = header[0];
    uint32_t symbol_offset = header[1];
    uint32_t bloom_count = header[2];

    if (bucket_count == 0) return symbol_offset;

    uint64_t buckets_vaddr = gnu_hash_vaddr + 4 * sizeof(uint32_t) + (uint64_t)bloom_count * sizeof(uint64_t);
    const uint32_t *buckets = (const uint32_t *)elf_vaddr_to_fileptr(
        image, buckets_vaddr, (uint64_t)bucket_count * sizeof(uint32_t)
    );
    if (!buckets) return 0;

    uint32_t last_bucket = 0;
    for (uint32_t i = 0; i < bucket_count; i++) {
        if (buckets[i] > last_bucket) last_bucket = buckets[i];
    }
    if (last_bucket < symbol_offset) return symbol_offset;

    uint64_t chains_vaddr = buckets_vaddr + (uint64_t)bucket_count * sizeof(uint32_t);
    uint64_t chain_index = last_bucket - symbol_offset;
    uint64_t count = last_bucket;

    for (uint64_t guard = 0; guard < 1000000; guard++) {
        const uint32_t *chain = (const uint32_t *)elf_vaddr_to_fileptr(
            image, chains_vaddr + chain_index * sizeof(uint32_t), sizeof(uint32_t)
        );
        if (!chain) return 0;

        count++;
        if (*chain & 1) break;
        chain_index++;
    }

    return count;
}

elf_load_status_t elf_process_dynamic(const elf_image_t *image, uint64_t *l4_table, uint64_t load_bias,
                                      const elf64_phdr_t *dynamic_ph, const elf_object_t *external_modules,
                                      int external_count, bool apply_relocs, elf_load_result_t *result) {
    if (!elf_range_within(dynamic_ph->p_offset, dynamic_ph->p_filesz, image->size))
        return ELF_ERR_TRUNCATED;

    elf_dynamic_t dynamic = {
        .rela_ent = sizeof(elf64_rela_t),
        .relr_ent = sizeof(uint64_t),
        .pltrel_type = DT_RELA,
    };
    parse_dynamic_tags(image, dynamic_ph, &dynamic);

    if (dynamic.has_legacy_rel && dynamic.rela_vaddr == 0) return ELF_ERR_BAD_DYNAMIC;

    if (dynamic.gnu_hash_vaddr)
        dynamic.sym_count = gnu_hash_symbol_count(image, dynamic.gnu_hash_vaddr);
    if (dynamic.sym_count == 0 && dynamic.hash_vaddr)
        dynamic.sym_count = sysv_hash_symbol_count(image, dynamic.hash_vaddr);

    result->symtab_vaddr = dynamic.symtab_vaddr;
    result->strtab_vaddr = dynamic.strtab_vaddr;
    result->strtab_size = dynamic.strtab_size;
    result->sym_count = dynamic.sym_count;

    if (!apply_relocs) return ELF_OK;

    if (dynamic.rela_vaddr && dynamic.rela_size) {
        int rc = elf_apply_rela(image, l4_table, load_bias, dynamic.rela_vaddr, dynamic.rela_size,
                                dynamic.rela_ent, &dynamic, external_modules, external_count);
        if (rc == -1) return ELF_ERR_DYNTABLE_NOT_FOUND;
        if (rc != 0) return ELF_ERR_RELOC_TARGET_UNMAPPED;
    }

    if (dynamic.jmprel_vaddr && dynamic.jmprel_size && dynamic.pltrel_type == DT_RELA) {
        if (elf_apply_rela(image, l4_table, load_bias, dynamic.jmprel_vaddr, dynamic.jmprel_size,
                           sizeof(elf64_rela_t), &dynamic, external_modules, external_count) != 0) {
            return ELF_ERR_RELOC_TARGET_UNMAPPED;
        }
    }

    if (dynamic.relr_vaddr && dynamic.relr_size) {
        if (dynamic.relr_ent != sizeof(uint64_t)) return ELF_ERR_BAD_DYNAMIC;
        if (elf_apply_relr(image, l4_table, load_bias, dynamic.relr_vaddr, dynamic.relr_size,
                           dynamic.relr_ent) != 0) {
            return ELF_ERR_RELOC_TARGET_UNMAPPED;
        }
    }

    return ELF_OK;
}
