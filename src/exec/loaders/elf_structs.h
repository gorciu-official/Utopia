#pragma once 

#include <types.h>
#include "elf_constants.h"

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
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

typedef struct {
    int64_t  d_tag;
    union {
        uint64_t d_val;
        uint64_t d_ptr;
    } d_un;
} __attribute__((packed)) elf64_dyn_t;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} __attribute__((packed)) elf64_rela_t;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} __attribute__((packed)) elf64_sym_t;

typedef struct {
    const uint8_t *data;
    uint64_t size;

    const elf64_ehdr_t *ehdr;
    const elf64_phdr_t *phdrs;
    uint16_t phnum;
    uint16_t phentsize;
} elf_image_t;

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

typedef struct {
    elf_image_t image;

    uint64_t load_bias;

    uint64_t symtab_vaddr;
    uint64_t strtab_vaddr;
    uint64_t strtab_size;
    uint64_t sym_count;
} elf_object_t;
