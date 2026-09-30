#pragma once

#define ELF_MAG0        0x7f
#define ELF_MAG1        'E'
#define ELF_MAG2        'L'
#define ELF_MAG3        'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EM_X86_64       0x3e
#define EM_RISCV64      243

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

#define ELF_PIE_BASE     0x555555554000ULL
#define ELF_INTERP_BASE  0x7f0000000000ULL
#define ELF_INTERP_PATH_MAX 256

#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ    6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_RANDOM   25
#define AT_EXECFN 31

#define ELF_PUSH_STACK(sp, value) \
    *sp -= 8; \
    *(uint64_t*)(*sp) = value; 
