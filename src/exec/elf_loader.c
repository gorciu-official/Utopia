#include <types.h>
#include <constants.h>
#include <memory.h>
#include <arch/x86_64/common.h>
#include <process.h>
#include <scheduler.h>
#include <lib/screen.h>
#include <drivers/filesystem.h>

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

typedef struct {
    int64_t  d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} __attribute__((packed)) Elf64_Dyn;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} __attribute__((packed)) Elf64_Rela;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} __attribute__((packed)) Elf64_Sym;

typedef enum {
    ELF_OK = 0,
    ELF_ERR_BAD_MAGIC,
    ELF_ERR_BAD_CLASS,
    ELF_ERR_BAD_ENDIAN,
    ELF_ERR_BAD_MACHINE,
    ELF_ERR_BAD_TYPE,
    ELF_ERR_TRUNCATED,
    ELF_ERR_NO_MEMORY,
    ELF_ERR_MAP_FAILED,
    ELF_ERR_BAD_DYNAMIC,
    ELF_ERR_RELOC_TARGET_UNMAPPED,
    ELF_ERR_DYNTABLE_NOT_FOUND,
    ELF_ERR_INTERP_TOO_LONG,
    ELF_ERR_INTERP_NOT_FOUND,
    ELF_ERR_INTERP_LOAD_FAILED,
} elf_load_status_t;

#define ELF_PIE_BASE     0x555555554000ULL
#define ELF_INTERP_BASE  0x7f0000000000ULL
#define ELF_INTERP_PATH_MAX 256

typedef struct {
    elf_load_status_t status;

    uint64_t entry;
    uint64_t load_bias; 
    uint64_t lowest_vaddr; 
    uint64_t highest_vaddr;

    uint64_t phdr_vaddr;  
    uint16_t phnum;    
    uint16_t phentsize; 

    bool is_pie;    
    bool has_interp;
    char interp_path[ELF_INTERP_PATH_MAX];

    uint64_t symtab_vaddr;
    uint64_t strtab_vaddr;
    uint64_t strtab_size;
    uint64_t sym_count;   
} elf_load_result_t;

#define ELF_MAG0        0x7f
#define ELF_MAG1        'E'
#define ELF_MAG2        'L'
#define ELF_MAG3        'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EM_X86_64       0x3e

#define ET_EXEC         2
#define ET_DYN          3

#define PT_NULL         0
#define PT_LOAD         1
#define PT_DYNAMIC      2
#define PT_INTERP       3
#define PT_PHDR         6  

#define PF_X            0x1
#define PF_W            0x2
#define PF_R            0x4

#define SHN_UNDEF       0

#define DT_NULL         0
#define DT_NEEDED       1
#define DT_PLTRELSZ     2
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_RELAENT      9
#define DT_STRSZ        10
#define DT_SYMENT       11
#define DT_REL          17
#define DT_RELSZ        18
#define DT_RELENT       19
#define DT_PLTREL       20
#define DT_RELRSZ       35
#define DT_RELR         36
#define DT_RELRENT      37
#define DT_JMPREL 23
#define DT_GNU_HASH     0x6ffffef5

#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8

#define ELF64_R_SYM(i)  ((i) >> 32)      // oh we all love c macros, (x) instead of x is not a bug,
#define ELF64_R_TYPE(i) ((uint32_t)(i))  // it is a feature

#ifndef PTE_ADDR_MASK
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#endif
#ifndef PAGE_PRESENT
#define PAGE_PRESENT  0x1ULL
#endif

typedef struct {
    const uint8_t* image;
    uint64_t       image_size;
    const uint8_t* phdr_base;
    uint16_t       phnum;
    uint16_t       phentsize;
    uint64_t       load_bias;

    uint64_t symtab_vaddr; 
    uint64_t strtab_vaddr;
    uint64_t strtab_size;  
    uint64_t sym_count;   
} elf_module_t;

static uint64_t page_flags_for(uint32_t p_flags) {
    uint64_t flags = 0;
    if (p_flags & PF_W) flags |= PAGE_RW;
    flags |= PAGE_USER;
    if (!(p_flags & PF_X)) flags |= PAGE_NX;
    return flags;
}

static void* elf_uv2kv(uint64_t *l4_table, uint64_t vaddr) {
    uint64_t pml4_i = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_i = (vaddr >> 30) & 0x1FF;
    uint64_t pd_i   = (vaddr >> 21) & 0x1FF;
    uint64_t pt_i   = (vaddr >> 12) & 0x1FF;

    if (!(l4_table[pml4_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pdpt = (uint64_t *)phys_to_virt(l4_table[pml4_i] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pd = (uint64_t *)phys_to_virt(pdpt[pdpt_i] & PTE_ADDR_MASK);

    if (!(pd[pd_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pt = (uint64_t *)phys_to_virt(pd[pd_i] & PTE_ADDR_MASK);

    if (!(pt[pt_i] & PAGE_PRESENT)) return NULL;
    uint64_t phys = pt[pt_i] & PTE_ADDR_MASK;

    return (void*)(phys_to_virt(phys) + (vaddr & 0xFFF));
}

static uint64_t* elf_walk_pte_slot(uint64_t *l4_table, uint64_t vaddr) {
    uint64_t pml4_i = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_i = (vaddr >> 30) & 0x1FF;
    uint64_t pd_i   = (vaddr >> 21) & 0x1FF;
    uint64_t pt_i   = (vaddr >> 12) & 0x1FF;

    if (!(l4_table[pml4_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pdpt = (uint64_t *)phys_to_virt(l4_table[pml4_i] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pd = (uint64_t *)phys_to_virt(pdpt[pdpt_i] & PTE_ADDR_MASK);

    if (!(pd[pd_i] & PAGE_PRESENT)) return NULL;
    uint64_t* pt = (uint64_t *)phys_to_virt(pd[pd_i] & PTE_ADDR_MASK);

    return &pt[pt_i];
}

static const void* elf_vaddr_to_fileptr(
    const uint8_t* image, uint64_t image_size, 
    const uint8_t* phdr_base, uint16_t phnum, uint16_t phentsize, 
    uint64_t vaddr, uint64_t need_size
) {
    for (uint16_t i = 0; i < phnum; i++) {
        const Elf64_Phdr* ph = (const Elf64_Phdr *)(phdr_base + (uint64_t)i * phentsize);
        if (ph->p_type != PT_LOAD) continue;

        uint64_t p_vaddr = ph->p_vaddr;

        if (vaddr >= p_vaddr && vaddr < p_vaddr + ph->p_filesz) {
            uint64_t offset_in_phdr = vaddr - p_vaddr;
            if (need_size <= ph->p_filesz - offset_in_phdr) {
                uint64_t off = ph->p_offset + offset_in_phdr;
                if (off + need_size <= image_size) {
                    return image + off;
                }
            }
        }
    }


    return NULL;
}

static const char* elf_symbol_name_raw(
        const uint8_t* image, uint64_t image_size, const uint8_t *phdr_base,
        uint16_t phnum, uint16_t phentsize,
        uint64_t strtab_vaddr, uint64_t strtab_size, uint32_t st_name
) {
    if (!strtab_vaddr || st_name >= strtab_size) return NULL;
    return (const char*)elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize,
                                               strtab_vaddr + st_name, 1);
}

static bool elf_streq(const char* a, const char *b) {
    for (uint64_t i = 0; i < 512; i++) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
    }
    return false;
}

static bool elf_module_find_symbol(const elf_module_t* mod, const char *name, uint64_t *out_value) {
    if (!mod->symtab_vaddr || !mod->strtab_vaddr || mod->sym_count == 0) return false;

    for (uint64_t i = 1; i < mod->sym_count; i++) { // index 0 is always the null symbol
        const Elf64_Sym* sym = (const Elf64_Sym *)elf_vaddr_to_fileptr(
            mod->image, mod->image_size, mod->phdr_base, mod->phnum, mod->phentsize,
            mod->symtab_vaddr + i * sizeof(Elf64_Sym), sizeof(Elf64_Sym));
        if (!sym) break;
        if (sym->st_name == 0 || sym->st_shndx == SHN_UNDEF) continue; // not defined here either

        const char* cand = elf_symbol_name_raw(mod->image, mod->image_size, mod->phdr_base,
                                                mod->phnum, mod->phentsize,
                                                mod->strtab_vaddr, mod->strtab_size, sym->st_name);
        if (!cand) continue;

        if (elf_streq(cand, name)) {
            *out_value = mod->load_bias + sym->st_value;
            return true;
        }
    }
    return false;
}

static bool elf_resolve_external_symbol(const elf_module_t* modules, int module_count, const char *name, uint64_t *out_value) {
    for (int i = 0; i < module_count; i++) {
        if (elf_module_find_symbol(&modules[i], name, out_value)) return true;
    }
    return false;
}

static int elf_apply_rela_table(
        const uint8_t* image, uint64_t image_size, const uint8_t *phdr_base, uint16_t phnum, uint16_t phentsize, uint64_t *l4_table,
        uint64_t load_bias, uint64_t table_vaddr, uint64_t table_size, uint64_t entsize, uint64_t symtab_vaddr,
        uint64_t strtab_vaddr, uint64_t strtab_size,
        const elf_module_t* ext_modules, int ext_module_count
) {
    if (entsize == 0) entsize = sizeof(Elf64_Rela);
    uint64_t count = table_size / entsize;

    const uint8_t* table = (const uint8_t*)elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize, table_vaddr, table_size);

    if (!table) {
        return -1;
    }

    for (uint64_t i = 0; i < count; i++) {
        const Elf64_Rela* rel = (const Elf64_Rela *)(table + i * entsize);
        uint32_t type = ELF64_R_TYPE(rel->r_info);
        uint64_t sym_idx = ELF64_R_SYM(rel->r_info);

        void* dst = elf_uv2kv(l4_table, load_bias + rel->r_offset);
        if (!dst)
            return -2; // relocation target isn't in any mapped PT_LOAD segment

        switch (type) {
            case R_X86_64_RELATIVE:
                *(uint64_t *)dst = load_bias + (uint64_t)rel->r_addend;
                break;

            case R_X86_64_64:
            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT: {
                if (symtab_vaddr == 0) break;
                const Elf64_Sym* sym = (const Elf64_Sym *)elf_vaddr_to_fileptr(
                    image, image_size, phdr_base, phnum, phentsize,
                    symtab_vaddr + sym_idx * sizeof(Elf64_Sym), sizeof(Elf64_Sym));
                if (!sym) break;

                if (sym->st_shndx == SHN_UNDEF) {
                    const char* name = elf_symbol_name_raw(image, image_size, phdr_base, phnum, phentsize,
                                                             strtab_vaddr, strtab_size, sym->st_name);

                    uint64_t resolved;
                    if (!name || !ext_modules || ext_module_count == 0 ||
                        !elf_resolve_external_symbol(ext_modules, ext_module_count, name, &resolved)) {
                        break;
                    }

                    if (type == R_X86_64_64) resolved += (uint64_t)rel->r_addend;
                    *(uint64_t *)dst = resolved;
                    break;
                }

                uint64_t value = load_bias + sym->st_value;
                if (type == R_X86_64_64) value += (uint64_t)rel->r_addend;
                *(uint64_t *)dst = value;
                break;
            }

            case R_X86_64_NONE:
                break;

            default:
                break;
        }
    }

    return 0;
}

static int elf_apply_relr_table(
        const uint8_t* image, uint64_t image_size, const uint8_t *phdr_base,
        uint16_t phnum, uint16_t phentsize, uint64_t* l4_table,
        uint64_t load_bias, uint64_t table_vaddr, uint64_t table_size, uint64_t entsize
) {
    if (entsize == 0) entsize = sizeof(uint64_t);
    uint64_t count = table_size / entsize;

    const uint8_t* table = (const uint8_t *)elf_vaddr_to_fileptr(
        image, image_size, phdr_base, phnum, phentsize, table_vaddr, table_size);
    if (!table) return -1;

    const uint64_t* entries = (const uint64_t *)table;
    uint64_t base = 0;

    for (uint64_t i = 0; i < count; i++) {
        uint64_t entry = entries[i];

        if ((entry & 1) == 0) {
            void* dst = elf_uv2kv(l4_table, load_bias + entry);
            if (!dst) return -2;
            *(uint64_t *)dst = load_bias + *(uint64_t *)dst;   // read existing value, add bias, write back
            base = entry + sizeof(uint64_t);
        } else {
            // odd entry: bitmap of the next 63 words starting at `base`
            uint64_t bits = entry >> 1;
            uint64_t addr = base;
            while (bits) {
                if (bits & 1) {
                    void* dst = elf_uv2kv(l4_table, load_bias + addr);
                    if (!dst) return -2;
                    *(uint64_t *)dst = load_bias + *(uint64_t *)dst; // add bias to existing stored value
                }
                bits >>= 1;
                addr += sizeof(uint64_t);
            }
            base += 63 * sizeof(uint64_t);
        }
    }
    return 0;
}

static bool elf_gnu_hash_symcount(
        const uint8_t* image, uint64_t image_size, const uint8_t *phdr_base,
        uint16_t phnum, uint16_t phentsize, uint64_t gnu_hash_vaddr, uint64_t *out_count
) {
    const void* hp = elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize,
                                           gnu_hash_vaddr, 4 * sizeof(uint32_t));
    if (!hp) return false;

    const uint32_t* header = (const uint32_t *)hp;
    uint32_t nbucket    = header[0];
    uint32_t symoffset  = header[1];
    uint32_t bloom_size = header[2];

    if (nbucket == 0) {
        *out_count = symoffset;
        return true;
    }

    uint64_t buckets_vaddr = gnu_hash_vaddr + 4 * sizeof(uint32_t) + (uint64_t)bloom_size * sizeof(uint64_t);

    const void* bp = elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize,
                                           buckets_vaddr, (uint64_t)nbucket * sizeof(uint32_t));
    if (!bp) return false;
    const uint32_t* buckets = (const uint32_t *)bp;

    uint32_t max_idx = 0;
    for (uint32_t i = 0; i < nbucket; i++) {
        if (buckets[i] > max_idx) max_idx = buckets[i];
    }

    if (max_idx < symoffset) {
        *out_count = symoffset;
        return true;
    }

    uint64_t chain_vaddr = buckets_vaddr + (uint64_t)nbucket * sizeof(uint32_t);
    uint64_t chain_idx = max_idx - symoffset;
    uint64_t count = max_idx;

    for (uint64_t guard = 0; guard < 1000000; guard++) {
        const void* cp = elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize,
                                               chain_vaddr + chain_idx * sizeof(uint32_t), sizeof(uint32_t));
        if (!cp) return false;
        uint32_t h = *(const uint32_t *)cp;
        count++;
        if (h & 1) break; // end-of-chain marker
        chain_idx++;
    }

    *out_count = count;
    return true;
}

static elf_load_status_t elf_process_dynamic(
        const uint8_t* image, uint64_t image_size, const uint8_t *phdr_base, uint16_t phnum,
        uint16_t phentsize, uint64_t* l4_table, uint64_t load_bias, const Elf64_Phdr *dyn_ph,
        const elf_module_t* ext_modules, int ext_module_count, elf_load_result_t *result
) {
    if (dyn_ph->p_offset + dyn_ph->p_filesz > image_size) return ELF_ERR_TRUNCATED;

    const Elf64_Dyn* dyn = (const Elf64_Dyn *)(image + dyn_ph->p_offset);
    uint64_t dyn_count = dyn_ph->p_filesz / sizeof(Elf64_Dyn);

    uint64_t rela_off = 0, rela_size = 0, rela_ent = sizeof(Elf64_Rela);
    uint64_t relr_off = 0, relr_size = 0, relr_ent = sizeof(uint64_t);
    uint64_t jmprel_off = 0, jmprel_size = 0;
    uint64_t pltrel_type = DT_RELA;
    uint64_t symtab_off = 0;
    uint64_t strtab_off = 0, strtab_size = 0;
    uint64_t hash_off = 0;
    uint64_t gnu_hash_off = 0;
    bool has_rel = false; // legacy 32-bit-style REL

    for (uint64_t i = 0; i < dyn_count; i++) {
        switch (dyn[i].d_tag) {
            case DT_NULL: goto done_scanning; // yes we use labels in 2026
            case DT_RELA:    rela_off  = dyn[i].d_un.d_ptr; break;
            case DT_RELASZ:  rela_size = dyn[i].d_un.d_val; break;
            case DT_RELAENT: rela_ent  = dyn[i].d_un.d_val; break;
            case DT_SYMTAB:  symtab_off = dyn[i].d_un.d_ptr; break;
            case DT_STRTAB:  strtab_off = dyn[i].d_un.d_ptr; break;
            case DT_STRSZ:   strtab_size = dyn[i].d_un.d_val; break;
            case DT_HASH:    hash_off = dyn[i].d_un.d_ptr; break;
            case DT_GNU_HASH: gnu_hash_off = dyn[i].d_un.d_ptr; break;
            case DT_PLTRELSZ: jmprel_size = dyn[i].d_un.d_val; break;
            case DT_PLTREL:  pltrel_type = dyn[i].d_un.d_val; break;
            case DT_REL:     has_rel = true; break;
            case DT_RELRSZ:  relr_size = dyn[i].d_un.d_val; break;
            case DT_RELR:    relr_off  = dyn[i].d_un.d_ptr; break;
            case DT_RELRENT: relr_ent  = dyn[i].d_un.d_val; break;
            default: break;
        }
    }
done_scanning:
    for (uint64_t i = 0; i < dyn_count; i++) {
        if (dyn[i].d_tag == DT_NULL) break;
        if (dyn[i].d_tag == 23) jmprel_off = dyn[i].d_un.d_ptr;
    }

    if (has_rel && rela_off == 0) {
        return ELF_ERR_BAD_DYNAMIC;
    }

    result->symtab_vaddr = symtab_off;
    result->strtab_vaddr = strtab_off;
    result->strtab_size  = strtab_size;
    result->sym_count    = 0;

    if (gnu_hash_off) {
        uint64_t count = 0;
        if (elf_gnu_hash_symcount(image, image_size, phdr_base, phnum, phentsize, gnu_hash_off, &count)) {
            result->sym_count = count;
        }
    }
    if (result->sym_count == 0 && hash_off) {
        const void* hp = elf_vaddr_to_fileptr(image, image_size, phdr_base, phnum, phentsize, hash_off, 2 * sizeof(uint32_t));
        if (hp) {
            result->sym_count = ((const uint32_t *)hp)[1];
        }
    }

    if (rela_off && rela_size) {
        int rc = elf_apply_rela_table(image, image_size, phdr_base, phnum, phentsize, l4_table, load_bias,
                                       rela_off, rela_size, rela_ent, symtab_off,
                                       strtab_off, strtab_size, ext_modules, ext_module_count);
        if (rc == -1) return ELF_ERR_DYNTABLE_NOT_FOUND;
        if (rc != 0) return ELF_ERR_RELOC_TARGET_UNMAPPED;
    }

    if (jmprel_off && jmprel_size && pltrel_type == DT_RELA) {
        if (elf_apply_rela_table(image, image_size, phdr_base, phnum, phentsize,
                                  l4_table, load_bias, jmprel_off, jmprel_size, sizeof(Elf64_Rela),
                                  symtab_off, strtab_off, strtab_size, ext_modules, ext_module_count) != 0) {
            return ELF_ERR_RELOC_TARGET_UNMAPPED;
        }
    }

    if (relr_off && relr_size) {
        if (elf_apply_relr_table(image, image_size, phdr_base, phnum, phentsize,
                                  l4_table, load_bias, relr_off, relr_size, relr_ent) != 0) {
            return ELF_ERR_RELOC_TARGET_UNMAPPED;
        }
    }

    return ELF_OK;
}

elf_load_result_t elf_load(const uint8_t* image, uint64_t image_size,
                            uint64_t* l4_table, uint64_t load_bias,
                            const elf_module_t* ext_modules, int ext_module_count) {
    elf_load_result_t result = {0};

    if (image_size < sizeof(elf64_ehdr)) {
        result.status = ELF_ERR_TRUNCATED;
        return result;
    }

    const elf64_ehdr* ehdr = (const elf64_ehdr *)image;

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
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        result.status = ELF_ERR_BAD_TYPE;
        return result;
    }

    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > image_size) {
        result.status = ELF_ERR_TRUNCATED;
        return result;
    }

    const uint8_t* phdr_base = image + ehdr->e_phoff;

    result.is_pie = (ehdr->e_type == ET_DYN);
    if (ehdr->e_type == ET_EXEC) load_bias = 0;
    result.load_bias = load_bias;
    result.phnum = ehdr->e_phnum;
    result.phentsize = ehdr->e_phentsize;

    uint64_t lowest = ~0ULL;
    uint64_t highest = 0;
    const Elf64_Phdr* dyn_ph = NULL;
    bool have_interp = false;
    const Elf64_Phdr* interp_ph = NULL;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr* ph = (const Elf64_Phdr *)(phdr_base + (uint64_t)i * ehdr->e_phentsize);

        if (ph->p_type == PT_PHDR) {
            result.phdr_vaddr = load_bias + ph->p_vaddr;
            continue;
        }
        if (ph->p_type == PT_DYNAMIC) {
            dyn_ph = ph;
            continue;
        }
        if (ph->p_type == PT_INTERP) {
            have_interp = true;
            interp_ph = ph;
            continue;
        }
        if (ph->p_type != PT_LOAD) continue;

        if (ph->p_offset + ph->p_filesz > image_size) {
            result.status = ELF_ERR_TRUNCATED;
            return result;
        }

        uint64_t seg_start = (load_bias + ph->p_vaddr) & ~0xFFFULL;
        uint64_t seg_end   = (load_bias + ph->p_vaddr + ph->p_memsz + 0xFFFULL) & ~0xFFFULL;
        uint64_t flags = page_flags_for(ph->p_flags);

        for (uint64_t page_va = seg_start; page_va < seg_end; page_va += 0x1000) {
            uint64_t phys;
            uint8_t* dst;

            uint64_t* pte = elf_walk_pte_slot(l4_table, page_va);
            bool already_mapped = pte && (*pte & PAGE_PRESENT);

            if (already_mapped) {
                phys = *pte & PTE_ADDR_MASK;
                dst  = (uint8_t *)phys_to_virt(phys);

                uint64_t merged = *pte;
                if (flags & PAGE_RW)    merged |= PAGE_RW;
                if (!(flags & PAGE_NX)) merged &= ~PAGE_NX;
                *pte = merged;
            } else {
                phys = hhdm_virt_to_phys(pt_pool_alloc());
                if (!phys) {
                    result.status = ELF_ERR_NO_MEMORY;
                    return result;
                }

                dst = (uint8_t *)phys_to_virt(phys);
                for (int b = 0; b < 4096; b++) dst[b] = 0;

                if (map_page_4k(l4_table, page_va, phys, flags) != 0) {
                    result.status = ELF_ERR_MAP_FAILED;
                    return result;
                }
            }

            uint64_t page_end = page_va + 0x1000;
            uint64_t file_lo = load_bias + ph->p_vaddr;
            uint64_t file_hi = load_bias + ph->p_vaddr + ph->p_filesz;

            uint64_t copy_lo = (page_va > file_lo) ? page_va : file_lo;
            uint64_t copy_hi = (page_end < file_hi) ? page_end : file_hi;

            if (copy_hi > copy_lo) {
                uint64_t src_off = ph->p_offset + (copy_lo - file_lo);
                uint64_t dst_off = copy_lo - page_va;
                uint64_t len = copy_hi - copy_lo;
                for (uint64_t b = 0; b < len; b++) {
                    dst[dst_off + b] = image[src_off + b];
                }
            }
        }

        if (seg_start < lowest) lowest = seg_start;
        if (seg_end > highest) highest = seg_end;

        if (result.phdr_vaddr == 0 &&
            ehdr->e_phoff >= ph->p_offset &&
            ehdr->e_phoff - ph->p_offset <= ph->p_filesz) {
            result.phdr_vaddr = load_bias + ph->p_vaddr + (ehdr->e_phoff - ph->p_offset);
        }
    }

    if (highest == 0) {
        result.status = ELF_ERR_TRUNCATED;
        return result;
    }

    result.lowest_vaddr = lowest;
    result.highest_vaddr = highest;
    result.entry = load_bias + ehdr->e_entry;

    if (have_interp) {
        if (interp_ph->p_offset + interp_ph->p_filesz > image_size) {
            result.status = ELF_ERR_TRUNCATED;
            return result;
        }
        uint64_t len = interp_ph->p_filesz;
        if (len >= ELF_INTERP_PATH_MAX) {
            result.status = ELF_ERR_INTERP_TOO_LONG;
            return result;
        }
        for (uint64_t b = 0; b < len; b++) {
            result.interp_path[b] = (char)image[interp_ph->p_offset + b];
        }
        result.interp_path[len] = '\0';
        result.has_interp = true;
    }

    if (dyn_ph) {
        elf_load_status_t dyn_status = elf_process_dynamic(
            image, image_size, phdr_base, ehdr->e_phnum, ehdr->e_phentsize,
            l4_table, load_bias, dyn_ph, ext_modules, ext_module_count, &result);
        if (dyn_status != ELF_OK) {
            result.status = dyn_status;
            return result;
        }
    }

    result.status = ELF_OK;
    return result;
}
// TODO: let's move this vibecoded shit
static int elf_vfs_read_whole(const char* path, uint8_t **out_buf, uint64_t *out_size) {
    vnode_t* node = NULL;
    int rc = vfs_open(path, O_RDONLY, &node);
    if (rc != 0 || !node) return -1;

    if (node->type != VNODE_TYPE_FILE || !node->ops || !node->ops->read) {
        return -1;
    }

    uint64_t size = node->size;
    uint8_t* buf = (uint8_t *)malloc(size ? size : 1);
    if (!buf) return -1;

    uint64_t total = 0;
    while (total < size) {
        uint64_t bytes_read = 0;
        int r = node->ops->read(node, buf + total, size - total, total, &bytes_read);
        if (r != 0) {
            free(buf);
            return r;
        }
        if (bytes_read == 0) break;
        total += bytes_read;
    }

    *out_buf = buf;
    *out_size = total;
    return 0;
}

int elf_load_full(const uint8_t* image, uint64_t image_size, uint64_t *l4_table, elf_load_result_t *out_main, elf_auxv_info_t *out_auxv, uint64_t *out_entry) {
    if (image_size < sizeof(elf64_ehdr)) return -1;
    const elf64_ehdr* peek = (const elf64_ehdr *)image;

    uint64_t bias = (peek->e_type == ET_DYN) ? ELF_PIE_BASE : 0;

    elf_load_result_t main_res = elf_load(image, image_size, l4_table, bias, NULL, 0);
    if (out_main) *out_main = main_res;
    if (main_res.status != ELF_OK) return -1;

    elf_auxv_info_t auxv = {0};
    auxv.at_entry = main_res.entry;
    auxv.at_phdr  = main_res.phdr_vaddr;
    auxv.at_phent = main_res.phentsize;
    auxv.at_phnum = main_res.phnum;
    auxv.at_base  = 0;
    auxv.has_interp = main_res.has_interp;

    if (!main_res.has_interp) {
        if (out_auxv) *out_auxv = auxv;
        if (out_entry) *out_entry = main_res.entry;
        return 0;
    }

    uint8_t* interp_buf = NULL;
    uint64_t interp_size = 0;
    if (elf_vfs_read_whole(main_res.interp_path, &interp_buf, &interp_size) != 0) {
        printk("ELF Loader", "Failed to load an ELF interpretter");
        return -2; // ELF_ERR_INTERP_NOT_FOUND
    }
    elf_module_t main_module = {
        .image        = image,
        .image_size   = image_size,
        .phdr_base    = image + ((const elf64_ehdr *)image)->e_phoff,
        .phnum        = main_res.phnum,
        .phentsize    = main_res.phentsize,
        .load_bias    = main_res.load_bias,
        .symtab_vaddr = main_res.symtab_vaddr,
        .strtab_vaddr = main_res.strtab_vaddr,
        .strtab_size  = main_res.strtab_size,
        .sym_count    = main_res.sym_count,
    };

    elf_load_result_t interp_res = elf_load(interp_buf, interp_size, l4_table, ELF_INTERP_BASE, &main_module, 1);

    if (interp_res.status != ELF_OK) {
        printk("ELF Loader", "Could not load ELF program interpretter, error: %d", interp_res.status);
        return -3; // ELF_ERR_INTERP_LOAD_FAILED
    }

    auxv.at_base = interp_res.load_bias;
    if (out_auxv) *out_auxv = auxv;
    if (out_entry) *out_entry = interp_res.entry;

    return 0;
}

int elf_start(const uint8_t* elf, uintptr_t size) {
    uint64_t* proc_l4 = clone_page_table();
    if (!proc_l4) return -1;

    elf_load_result_t main_res;
    elf_auxv_info_t auxv;
    uint64_t entry;

    int rc = elf_load_full(elf, size, proc_l4, &main_res, &auxv, &entry);
    if (rc != 0) {
        free_page_table(proc_l4);
        return rc;
    }

    write_cr3(hhdm_virt_to_phys(proc_l4)); // TODO: context switch should do this not me

    process_t* proc = process_create("jakis-elf", (void (*)(void *))entry, NULL, 3, &auxv);
    if (!proc) {
        free_page_table(proc_l4);
        return -1;
    }

    if (proc->page_table) free_page_table(proc->page_table);
    proc->page_table = proc_l4;

    proc->brk_start = (main_res.highest_vaddr + 0xFFFULL) & ~0xFFFULL;
    proc->brk_current = proc->brk_start;
    proc->mmap_start = 0x400000000000;
    proc->mmap_current = proc->mmap_start;

    return 0;
}
