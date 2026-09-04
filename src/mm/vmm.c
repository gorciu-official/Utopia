#include <types.h>
#include <constants.h>
#include <lib/screen.h>
#include <memory.h>
#include <arch/memory.h>

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
#include <boot/limine.h>
#endif

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
extern uint64_t page_table_l4[];
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
uint64_t* page_table_l4 = NULL; 

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};
#endif

void vmm_init(void) {
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
    uint64_t root_phys;
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    asm volatile("mov %%cr3, %0" : "=r"(root_phys));
    root_phys &= 0x000FFFFFFFFFF000ULL;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    uint64_t satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    root_phys = (satp & 0x00000FFFFFFFFFFFULL) << 12; 
#endif
    page_table_l4 = (uint64_t*)phys_to_virt(root_phys);
#endif
}

uint64_t* pt_pool_alloc(void) {
    void* page = page_alloc(1);
    if (!page) return page;
    memset(page, 0, 4096);
    return page;
}

uintptr_t kernel_virt_to_phys(void* addr) {
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (exec_addr_request.response) {
        return (uintptr_t)addr - exec_addr_request.response->virtual_base + exec_addr_request.response->physical_base;
    }
#endif
    return (uintptr_t)addr - 0xffffffff80000000ULL + 0x100000ULL;
}

uintptr_t hhdm_virt_to_phys(void* addr) {
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (exec_addr_request.response) {
        return (uintptr_t)addr - hhdm_request.response->offset;
    }
#endif
    return (uintptr_t)addr - 0xffff800000000000;
}

void* phys_to_virt(uint64_t phys) {
#if BOOTLOADER == BOOTLOADER_CODE_GRUB
    return (void*)phys + 0xffff800000000000;
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (hhdm_request.response) {
        return (void*)(hhdm_request.response->offset + phys);
    }
    return (void*)phys;
#endif
}

static int split_leaf(pte_t* entry_slot, int level) {
    pte_t old = *entry_slot;
    uint64_t base_phys = arch_pte_phys(old);
    int child_level = level - 1;
    uint64_t child_span = 1ULL << (12 + child_level * 9);

    uint64_t* new_table = pt_pool_alloc();
    if (!new_table) return -1;

    for (int i = 0; i < PT_ENTRIES; i++)
        new_table[i] = arch_pte_child_leaf(old, base_phys + (uint64_t)i * child_span, child_level);

    *entry_slot = arch_pte_table((uint64_t)hhdm_virt_to_phys(new_table));
    return 0;
}

int map_physical_range(uint64_t phys_start, uint64_t size, vm_flags_t flags) {
    uint64_t end = phys_start + size;
    uint64_t huge_span = 1ULL << (12 + 1 * 9);

    for (uint64_t addr = phys_start; addr < end; addr += huge_span) {
        uint64_t* table = page_table_l4;

        for (int level = PT_TOP_LEVEL; level > 1; level--) {
            int idx = pt_index(addr, level);
            pte_t entry = table[idx];
            if (!arch_pte_present(entry)) {
                uint64_t* child = pt_pool_alloc();
                if (!child) return -1;
                table[idx] = arch_pte_table((uint64_t)hhdm_virt_to_phys(child));
                table = child;
            } else {
                table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
            }
        }

        int idx1 = pt_index(addr, 1);
        table[idx1] = arch_pte_leaf(addr & ~(huge_span - 1), flags, true);
    }

    return 0;
}

void set_page_permissions(uint64_t virt, vm_flags_t flags) {
    uint64_t* table = page_table_l4;
    for (int level = PT_TOP_LEVEL; level > 0; level--) {
        int idx = pt_index(virt, level);
        pte_t entry = table[idx];
        if (!arch_pte_present(entry)) return;
        if (arch_pte_is_leaf(entry, level)) {
            table[idx] = arch_pte_with_flags(entry, flags);
            arch_tlb_flush_page(virt);
            return;
        }
        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }
    int leaf_idx = pt_index(virt, 0);
    if (!arch_pte_present(table[leaf_idx])) return;
    table[leaf_idx] = arch_pte_with_flags(table[leaf_idx], flags);
    arch_tlb_flush_page(virt);
}

void set_page_executable(uint64_t virt, bool executable) {
    uint64_t* table = page_table_l4;
    for (int level = PT_TOP_LEVEL; level > 0; level--) {
        int idx = pt_index(virt, level);
        pte_t entry = table[idx];
        if (!arch_pte_present(entry)) return;
        if (arch_pte_is_leaf(entry, level)) {
            table[idx] = arch_pte_set_exec(entry, executable);
            arch_tlb_flush_page(virt);
            return;
        }
        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }
    int leaf_idx = pt_index(virt, 0);
    if (!arch_pte_present(table[leaf_idx])) return;
    table[leaf_idx] = arch_pte_set_exec(table[leaf_idx], executable);
    arch_tlb_flush_page(virt);
}

static void free_table_level(uint64_t* table, int level) {
    if (level == 0) return;
    for (int i = 0; i < PT_ENTRIES; i++) {
        pte_t entry = table[i];
        if (!arch_pte_present(entry)) continue;
        if (arch_pte_is_leaf(entry, level)) continue;
        uint64_t* child = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
        free_table_level(child, level - 1);
    }
}

void free_page_table(uint64_t* l4_table) {
    for (int i = 0; i < 256; i++) {
        pte_t entry = l4_table[i];
        if (!arch_pte_present(entry)) continue;
        uint64_t* l3 = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
        free_table_level(l3, PT_TOP_LEVEL - 1);
    }
}

static uint64_t* clone_table_level(uint64_t* old_table, int level) {
    uint64_t* new_table = pt_pool_alloc();
    if (!new_table) return NULL;

    for (int i = 0; i < PT_ENTRIES; i++) {
        pte_t entry = old_table[i];

        if (!arch_pte_present(entry)) { new_table[i] = 0; continue; }

        if (arch_pte_is_leaf(entry, level)) {
            new_table[i] = entry; 
            continue;
        }

        uint64_t* old_child = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
        uint64_t* new_child = clone_table_level(old_child, level - 1);
        if (!new_child) { free_table_level(new_table, level); return NULL; }

        new_table[i] = arch_pte_table((uint64_t)hhdm_virt_to_phys(new_child));
    }

    return new_table;
}

uint64_t* clone_page_table(void) {
    uint64_t* new_l4 = pt_pool_alloc();
    if (!new_l4) return NULL;

    for (int i = 0; i < PT_ENTRIES; i++) {
        if (i >= 256) { new_l4[i] = page_table_l4[i]; continue; }

        pte_t entry = page_table_l4[i];
        if (!arch_pte_present(entry)) { new_l4[i] = 0; continue; }

        uint64_t* old_l3 = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
        uint64_t* new_l3 = clone_table_level(old_l3, PT_TOP_LEVEL - 1);
        if (!new_l3) { free_page_table(new_l4); return NULL; }

        new_l4[i] = arch_pte_table((uint64_t)hhdm_virt_to_phys(new_l3));
    }

    return new_l4;
}

int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, vm_flags_t flags) {
    if (!l4_table) return -1;
    uint64_t* table = l4_table;

    for (int level = PT_TOP_LEVEL; level > 0; level--) {
        int idx = pt_index(virt, level);
        pte_t entry = table[idx];

        if (!arch_pte_present(entry)) {
            uint64_t* child = pt_pool_alloc();
            if (!child) return -1;
            table[idx] = arch_pte_table((uint64_t)hhdm_virt_to_phys(child));
            table = child;
            continue;
        }

        if (arch_pte_is_leaf(entry, level)) {
            uint64_t region = 1ULL << (12 + level * 9);
            uint64_t region_base = virt & ~(region - 1);
            if (split_leaf(&table[idx], level) != 0) return -1;
            arch_tlb_flush_range(region_base, region);
            entry = table[idx];
        }

        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }

    int leaf_idx = pt_index(virt, 0);
    table[leaf_idx] = arch_pte_leaf(phys, flags, false);
    arch_tlb_flush_page(virt);
    return 0;
}

int protect_page_4k(uint64_t* l4_table, uint64_t virt, vm_flags_t flags) {
    if (!l4_table) return -1;
    uint64_t* table = l4_table;

    for (int level = PT_TOP_LEVEL; level > 0; level--) {
        int idx = pt_index(virt, level);
        pte_t entry = table[idx];
        if (!arch_pte_present(entry)) return -1;

        if (arch_pte_is_leaf(entry, level)) {
            uint64_t region = 1ULL << (12 + level * 9);
            uint64_t region_base = virt & ~(region - 1);
            if (split_leaf(&table[idx], level) != 0) return -1;
            arch_tlb_flush_range(region_base, region);
            entry = table[idx];
        }
        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }

    int leaf_idx = pt_index(virt, 0);
    if (!arch_pte_present(table[leaf_idx])) return -1;
    uint64_t phys = arch_pte_phys(table[leaf_idx]);
    table[leaf_idx] = arch_pte_leaf(phys, flags, false);
    arch_tlb_flush_page(virt);
    return 0;
}

int unmap_page_4k(uint64_t* l4_table, uint64_t virt) {
    if (!l4_table) return -1;
    uint64_t* table = l4_table;

    for (int level = PT_TOP_LEVEL; level > 0; level--) {
        int idx = pt_index(virt, level);
        pte_t entry = table[idx];
        if (!arch_pte_present(entry)) return 0;

        if (arch_pte_is_leaf(entry, level)) {
            uint64_t region = 1ULL << (12 + level * 9);
            uint64_t region_base = virt & ~(region - 1);
            if (split_leaf(&table[idx], level) != 0) return -1;
            arch_tlb_flush_range(region_base, region);
            entry = table[idx];
        }
        table = (uint64_t*)phys_to_virt(arch_pte_phys(entry));
    }

    int leaf_idx = pt_index(virt, 0);
    table[leaf_idx] = 0;
    arch_tlb_flush_page(virt);
    return 0;
}
