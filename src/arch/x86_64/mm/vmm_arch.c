#include <arch/memory.h>

#define PAGE_PRESENT       (1ULL << 0)
#define PAGE_RW            (1ULL << 1)
#define PAGE_USER          (1ULL << 2)
#define PAGE_HUGE          (1ULL << 7)
#define PAGE_NX            (1ULL << 63)
#define PAGE_GLOBAL        0x100
#define PAGE_PHYS_MASK     0x000FFFFFFFFFF000ULL

pte_t arch_pte_table(uint64_t child_phys) {
    return (child_phys & PAGE_PHYS_MASK) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
}

pte_t arch_pte_leaf(uint64_t phys, vm_flags_t flags, bool huge) {
    pte_t pte = (phys & PAGE_PHYS_MASK) | PAGE_PRESENT;
    if (flags & VMF_WRITE)   pte |= PAGE_RW;
    if (flags & VMF_USER)    pte |= PAGE_USER;
    if (!(flags & VMF_EXEC)) pte |= PAGE_NX;
    if (flags & VMF_GLOBAL)  pte |= PAGE_GLOBAL;
    if (huge)                pte |= PAGE_HUGE;
    return pte;
}

pte_t arch_pte_with_flags(pte_t existing, vm_flags_t flags) {
    pte_t pte = existing & (PAGE_PHYS_MASK | PAGE_HUGE | PAGE_PRESENT);
    if (flags & VMF_WRITE)   pte |= PAGE_RW;
    if (flags & VMF_USER)    pte |= PAGE_USER;
    if (!(flags & VMF_EXEC)) pte |= PAGE_NX;
    if (flags & VMF_GLOBAL)  pte |= PAGE_GLOBAL;
    return pte;
}

pte_t arch_pte_set_exec(pte_t pte, bool executable) {
    return executable ? (pte & ~PAGE_NX) : (pte | PAGE_NX);
}

pte_t arch_pte_child_leaf(pte_t parent, uint64_t child_phys, int child_level) {
    uint64_t flag_bits = parent & (0xFFFULL | PAGE_NX);
    pte_t pte = (child_phys & PAGE_PHYS_MASK) | flag_bits;
    if (child_level > 0) pte |= PAGE_HUGE; else pte &= ~PAGE_HUGE;
    return pte;
}

bool arch_pte_present(pte_t pte) { return pte & PAGE_PRESENT; }

bool arch_pte_is_leaf(pte_t pte, int level) {
    return level == 0 || (pte & PAGE_HUGE);
}

uint64_t arch_pte_phys(pte_t pte) { return pte & PAGE_PHYS_MASK; }

void arch_tlb_flush_page(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void arch_tlb_flush_range(uint64_t virt_start, uint64_t size) {
    for (uint64_t addr = virt_start & ~0xFFFULL; addr < virt_start + size; addr += 0x1000)
        arch_tlb_flush_page(addr);
}
