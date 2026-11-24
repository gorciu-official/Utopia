#include <types.h>
#include <mm/pmm.h>
#include <fs/filesystem.h>
#include <lib/screen.h>

#include "elf_internal.h"

static int read_whole_file(const char *path, uint8_t **out_data, uint64_t *out_size) {
    vnode_t *node = NULL;
    int rc = vfs_open(path, O_RDONLY, &node);
    if (rc != 0 || !node) return -1;
    if (node->type != VNODE_TYPE_FILE || !node->ops || !node->ops->read) return -1;

    uint64_t size = node->size;
    uint8_t *data = (uint8_t *)malloc(size ? size : 1);
    if (!data) return -1;

    uint64_t total = 0;
    while (total < size) {
        uint64_t bytes_read = 0;
        if (node->ops->read(node, data + total, size - total, total, &bytes_read) != 0) {
            free(data);
            return -1;
        }
        if (bytes_read == 0) break;
        total += bytes_read;
    }

    *out_data = data;
    *out_size = total;
    return 0;
}

static elf_auxv_info_t build_auxv(const elf_load_result_t *main_result) {
    return (elf_auxv_info_t){
        .at_entry = main_result->entry,
        .at_phdr = main_result->phdr_vaddr,
        .at_phent = main_result->phentsize,
        .at_phnum = main_result->phnum,
        .at_base = 0,
        .has_interp = main_result->has_interp,
    };
}

static elf_load_status_t load_interpreter(const elf_load_result_t *main_result,
                                          const elf_image_t *main_image, uint64_t *l4_table,
                                          elf_load_result_t *out_interp) {
    uint8_t *interp_data = NULL;
    uint64_t interp_size = 0;

    if (read_whole_file(main_result->interp_path, &interp_data, &interp_size) != 0) {
        printk(KMT_LOG, "ELF Loader", "Failed to load an ELF interpretter, path %s", main_result->interp_path);
        return ELF_ERR_INTERP_LOAD_FAILED;
    }

    elf_object_t main_object = {
        .image = *main_image,
        .load_bias = main_result->load_bias,
        .symtab_vaddr = main_result->symtab_vaddr,
        .strtab_vaddr = main_result->strtab_vaddr,
        .strtab_size = main_result->strtab_size,
        .sym_count = main_result->sym_count,
    };

    elf_load_status_t status = elf_load_image(interp_data, interp_size, l4_table, ELF_INTERP_BASE,
                                              &main_object, 1, false, out_interp);
    free(interp_data);

    if (status != ELF_OK) {
        printk(KMT_LOG, "ELF Loader", "Could not load ELF program interpretter, error: %d", status);
        return ELF_ERR_INTERP_LOAD_FAILED;
    }
    return ELF_OK;
}

int elf_load_full(const uint8_t *image, uint64_t image_size, uint64_t *l4_table,
                  elf_load_result_t *out_main, elf_auxv_info_t *out_auxv, uint64_t *out_entry) {
    if (image_size < sizeof(elf64_ehdr_t)) return -1;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)image;
    uint64_t load_bias = ehdr->e_type == ET_DYN ? ELF_PIE_BASE : 0;

    elf_load_result_t main_result;
    elf_load_status_t status = elf_load_image(image, image_size, l4_table, load_bias,
                                              NULL, 0, false, &main_result);

    if (out_main) *out_main = main_result;
    if (status != ELF_OK) {
        printk(KMT_LOG, "ELF Loader", "Failed to load main ELF image, status %d", status);
        return -1;
    }

    elf_auxv_info_t auxv = build_auxv(&main_result);

    if (!main_result.has_interp) {
        if (out_auxv) *out_auxv = auxv;
        if (out_entry) *out_entry = main_result.entry;
        return 0;
    }

    elf_image_t main_image = {
        .data = image,
        .size = image_size,
        .ehdr = ehdr,
        .phdrs = (const elf64_phdr_t *)(image + ehdr->e_phoff),
        .phnum = main_result.phnum,
        .phentsize = main_result.phentsize,
    };

    elf_load_result_t interp_result;
    status = load_interpreter(&main_result, &main_image, l4_table, &interp_result);
    if (status != ELF_OK) return status;

    auxv.at_base = interp_result.load_bias;
    if (out_auxv) *out_auxv = auxv;
    if (out_entry) *out_entry = interp_result.entry;
    return 0;
}
