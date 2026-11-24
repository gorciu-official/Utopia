#include <types.h>
#include <constants.h>
#include <lib/screen.h>

#include "elf_internal.h"

elf_load_status_t elf_validate_header(const uint8_t *data, uint64_t size, const elf64_ehdr_t **out_ehdr) {
    if (size < sizeof(elf64_ehdr_t)) return ELF_ERR_TRUNCATED;

    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)data;

    if (ehdr->e_ident[0] != ELF_MAG0 || ehdr->e_ident[1] != ELF_MAG1 ||
        ehdr->e_ident[2] != ELF_MAG2 || ehdr->e_ident[3] != ELF_MAG3) {
        return ELF_ERR_BAD_MAGIC;
    }
    if (ehdr->e_ident[4] != ELFCLASS64) return ELF_ERR_BAD_CLASS;
    if (ehdr->e_ident[5] != ELFDATA2LSB) return ELF_ERR_BAD_ENDIAN;

    uint16_t machine = ARCHITECTURE == ARCHITECTURE_CODE_x86_64 ? EM_X86_64 : EM_RISCV64;
    if (ehdr->e_machine != machine) {
        printk(KMT_LOG, "ELF Loader", "Tried to load an executable that is incompatibile with this processor architecture");
        printk(KMT_LOG, "ELF Loader", "This may occur if you use the same initramfs across different architectures");
        return ELF_ERR_BAD_MACHINE;
    }
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) return ELF_ERR_BAD_TYPE;

    if (ehdr->e_phentsize < sizeof(elf64_phdr_t)) return ELF_ERR_TRUNCATED;

    uint64_t phdr_size = (uint64_t)ehdr->e_phnum * ehdr->e_phentsize;
    if (!elf_range_within(ehdr->e_phoff, phdr_size, size)) return ELF_ERR_TRUNCATED;

    *out_ehdr = ehdr;
    return ELF_OK;
}
