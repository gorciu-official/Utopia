#include <types.h>
#include <arch/memory.h>

#define RV_PTE_V (1ULL << 0)
#define RV_PTE_R (1ULL << 1)
#define RV_PTE_W (1ULL << 2)
#define RV_PTE_X (1ULL << 3)
#define RV_PTE_U (1ULL << 4)
#define RV_PTE_G (1ULL << 5)
#define RV_PTE_A (1ULL << 6)
#define RV_PTE_D (1ULL << 7)
#define RV_PPN_SHIFT 10

static inline uint64_t rv_phys_to_ppn(uint64_t phys) { return (phys >> 12) << RV_PPN_SHIFT; }
static inline uint64_t rv_ppn_to_phys(pte_t pte)      { return (pte >> RV_PPN_SHIFT) << 12; }

pte_t arch_pte_table(uint64_t child_phys) {
    return rv_phys_to_ppn(child_phys) | RV_PTE_V;
}

pte_t arch_pte_leaf(uint64_t phys, vm_flags_t flags, bool huge) {
    (void)huge;
    pte_t pte = rv_phys_to_ppn(phys) | RV_PTE_V | RV_PTE_A | RV_PTE_D | RV_PTE_R;
    if (flags & VMF_WRITE)  pte |= RV_PTE_W;
    if (flags & VMF_EXEC)   pte |= RV_PTE_X;
    if (flags & VMF_USER)   pte |= RV_PTE_U;
    if (flags & VMF_GLOBAL) pte |= RV_PTE_G;
    return pte;
}

pte_t arch_pte_with_flags(pte_t existing, vm_flags_t flags) {
    pte_t pte = (existing & ~(RV_PTE_R | RV_PTE_W | RV_PTE_X | RV_PTE_U | RV_PTE_G))
                | RV_PTE_V | RV_PTE_A | RV_PTE_D | RV_PTE_R;
    if (flags & VMF_WRITE)  pte |= RV_PTE_W;
    if (flags & VMF_EXEC)   pte |= RV_PTE_X;
    if (flags & VMF_USER)   pte |= RV_PTE_U;
    if (flags & VMF_GLOBAL) pte |= RV_PTE_G;
    return pte;
}

pte_t arch_pte_set_exec(pte_t pte, bool executable) {
    return executable ? (pte | RV_PTE_X) : (pte & ~RV_PTE_X);
}

pte_t arch_pte_child_leaf(pte_t parent, uint64_t child_phys, int child_level) {
    (void)child_level;
    uint64_t flag_bits = parent & 0x3FF; 
    return rv_phys_to_ppn(child_phys) | flag_bits;
}

bool arch_pte_present(pte_t pte) { return pte & RV_PTE_V; }

bool arch_pte_is_leaf(pte_t pte, int level) {
    return level == 0 || (pte & (RV_PTE_R | RV_PTE_W | RV_PTE_X));
}

uint64_t arch_pte_phys(pte_t pte) { return rv_ppn_to_phys(pte); }

void arch_tlb_flush_page(uint64_t virt) {
    __asm__ volatile("sfence.vma %0, zero" :: "r"(virt) : "memory");
}

void arch_tlb_flush_range(uint64_t virt_start, uint64_t size) {
    if (size > 0x40000) { __asm__ volatile("sfence.vma" ::: "memory"); return; }
    for (uint64_t addr = virt_start & ~0xFFFULL; addr < virt_start + size; addr += 0x1000)
        arch_tlb_flush_page(addr);
}
