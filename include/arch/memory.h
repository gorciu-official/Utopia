#pragma once 

#include <types.h>

typedef enum {
    VMF_WRITE  = 1 << 0,
    VMF_USER   = 1 << 1,
    VMF_EXEC   = 1 << 2,  
    VMF_GLOBAL = 1 << 3,
} vm_flags_t;

typedef uint64_t pte_t;

#define PT_LEVELS     4
#define PT_TOP_LEVEL  3
#define PT_ENTRIES    512

static inline int pt_index(uint64_t virt, int level) {
    return (int)((virt >> (12 + level * 9)) & 0x1FF);
}

static inline bool pt_level_allows_huge(int level) {
    return level == 1 || level == 2;
}

pte_t    arch_pte_table(uint64_t child_phys);
pte_t    arch_pte_leaf(uint64_t phys, vm_flags_t flags, bool huge);
pte_t    arch_pte_with_flags(pte_t existing, vm_flags_t flags);
pte_t    arch_pte_set_exec(pte_t existing, bool executable);
pte_t    arch_pte_child_leaf(pte_t parent_huge, uint64_t child_phys, int child_level);
bool     arch_pte_present(pte_t pte);
bool     arch_pte_is_leaf(pte_t pte, int level);
uint64_t arch_pte_phys(pte_t pte);

void arch_tlb_flush_page(uint64_t virt);
void arch_tlb_flush_range(uint64_t virt_start, uint64_t size);
