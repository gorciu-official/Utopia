#include <types.h>
#include <constants.h>
#include <lib/screen.h>
#include <memory.h>

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
    uint64_t cr3_phys;
    asm volatile("mov %%cr3, %0" : "=r"(cr3_phys));
    page_table_l4 = (uint64_t*)phys_to_virt(cr3_phys & PAGE_PHYS_MASK);
#endif
}

#define PT_POOL_PAGES 128
static uint8_t pt_pool[PT_POOL_PAGES][4096] __attribute__((aligned(4096)));
static uint32_t pt_pool_next = 0;

uint64_t* pt_pool_alloc(void) {
    if (pt_pool_next >= PT_POOL_PAGES) return NULL;
    uint64_t* page = (uint64_t*)pt_pool[pt_pool_next++];
    memset(page, 0, 4096);
    return page;
}

uintptr_t kernel_virt_to_phys(void* addr) {
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (exec_addr_request.response) {
        return (uintptr_t)addr - exec_addr_request.response->virtual_base + exec_addr_request.response->physical_base;
    }
    return (uintptr_t)addr - 0xffffffff80000000ULL + 0x100000ULL;
#else
    return (uintptr_t)addr;
#endif
}

uintptr_t hhdm_virt_to_phys(void* addr) {
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (exec_addr_request.response) {
        return (uintptr_t)addr - hhdm_request.response->offset;
    }
#endif 
    return (uintptr_t)addr;
}

void* phys_to_virt(uint64_t phys) {
#if BOOTLOADER == BOOTLOADER_CODE_GRUB
    return (void*)phys;
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
    if (hhdm_request.response) {
        return (void*)(hhdm_request.response->offset + phys);
    }
    return (void*)phys;
#endif
}

int map_physical_range(uint64_t phys_start, uint64_t size, uint64_t flags) {
    uint64_t end = phys_start + size;

    for (uint64_t addr = phys_start; addr < end; addr += 0x200000) {
        uint64_t l4_idx = (addr >> 39) & 0x1FF;
        uint64_t l3_idx = (addr >> 30) & 0x1FF;
        uint64_t l2_idx = (addr >> 21) & 0x1FF;

        if (!(page_table_l4[l4_idx] & PAGE_PRESENT)) {
            uint64_t* new_l3 = pt_pool_alloc();
            if (!new_l3) return -1;
            page_table_l4[l4_idx] = (uint64_t)kernel_virt_to_phys(new_l3) | PAGE_PRESENT | PAGE_RW;
        }

        uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & PAGE_PHYS_MASK);

        if (!(l3[l3_idx] & PAGE_PRESENT)) {
            uint64_t* new_l2 = pt_pool_alloc();
            if (!new_l2) return -1;
            l3[l3_idx] = (uint64_t)kernel_virt_to_phys(new_l2) | PAGE_PRESENT | PAGE_RW;
        }

        uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & PAGE_PHYS_MASK);

        l2[l2_idx] = (addr & ~0x1FFFFFULL) | flags | PAGE_PRESENT | PAGE_RW | PAGE_HUGE;
    }

    return 0;
}

void set_page_permissions(uint64_t virt, uint64_t flags) {
    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;

    if (!(page_table_l4[l4_idx] & PAGE_PRESENT)) return;
    page_table_l4[l4_idx] |= (flags & (PAGE_USER | PAGE_RW));
    uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & PAGE_PHYS_MASK);

    if (!(l3[l3_idx] & PAGE_PRESENT)) return;
    l3[l3_idx] |= (flags & (PAGE_USER | PAGE_RW));
    if (l3[l3_idx] & PAGE_HUGE) {
        l3[l3_idx] = (l3[l3_idx] & PAGE_PHYS_MASK) | flags | PAGE_HUGE;
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
        return;
    }
    uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & PAGE_PHYS_MASK);

    if (l2[l2_idx] & PAGE_HUGE) {
        l2[l2_idx] = (l2[l2_idx] & PAGE_PHYS_MASK) | flags | PAGE_HUGE;
    } else if (l2[l2_idx] & PAGE_PRESENT) {
        l2[l2_idx] |= (flags & (PAGE_USER | PAGE_RW));
        uint64_t l1_idx = (virt >> 12) & 0x1FF;
        uint64_t* l1 = (uint64_t*)phys_to_virt(l2[l2_idx] & PAGE_PHYS_MASK);
        l1[l1_idx] = (l1[l1_idx] & PAGE_PHYS_MASK) | flags;
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void set_page_executable(uint64_t virt, bool executable) {
    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;

    if (!(page_table_l4[l4_idx] & PAGE_PRESENT))
        return;

    uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & PAGE_PHYS_MASK);

    if (!(l3[l3_idx] & PAGE_PRESENT))
        return;

    if (l3[l3_idx] & PAGE_HUGE) {
        if (executable) {
            l3[l3_idx] &= ~PAGE_NX;
        } else {
            l3[l3_idx] |= PAGE_NX;
        }
        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
        return;
    }

    uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & PAGE_PHYS_MASK);

    if (l2[l2_idx] & PAGE_HUGE) {

        if (executable) {
            l2[l2_idx] &= ~PAGE_NX;
        } else {
            l2[l2_idx] |= PAGE_NX;
        }

        __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
        return;
    }

    uint64_t l1_idx = (virt >> 12) & 0x1FF;
    uint64_t* l1 = (uint64_t*)phys_to_virt(l2[l2_idx] & PAGE_PHYS_MASK);

    if (!(l2[l2_idx] & PAGE_PRESENT))
        return;

    if (executable) {
        l1[l1_idx] &= ~PAGE_NX;
    } else {
        l1[l1_idx] |= PAGE_NX;
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

static void free_table_level(uint64_t* table, int level) {
    if (level > 0) {
        for (int i = 0; i < 512; i++) {
            uint64_t entry = table[i];
            if (!(entry & PAGE_PRESENT)) continue;
            if ((entry & PAGE_HUGE) || level == 1) continue; 
            uint64_t* child = (uint64_t*)phys_to_virt(entry & PAGE_PHYS_MASK);
            free_table_level(child, level - 1);
        }
    }
}

void free_page_table(uint64_t* l4_table) {
    for (int i = 0; i < 256; i++) {
        if (!(l4_table[i] & PAGE_PRESENT)) continue;
        uint64_t* l3 = (uint64_t*)phys_to_virt(l4_table[i] & PAGE_PHYS_MASK);
        free_table_level(l3, 3);
    }
}

static uint64_t* clone_table_level(uint64_t* old_table, int level) {
    uint64_t* new_table = pt_pool_alloc();
    if (!new_table) return NULL;

    for (int i = 0; i < 512; i++) {
        uint64_t entry = old_table[i];

        if (!(entry & PAGE_PRESENT)) {
            new_table[i] = 0;
            continue;
        }

        if ((entry & PAGE_HUGE) || level == 1) {
            new_table[i] = entry;
            continue;
        }

        uint64_t* old_child = (uint64_t*)phys_to_virt(entry & PAGE_PHYS_MASK);
        uint64_t* new_child = clone_table_level(old_child, level - 1);
        if (!new_child) {
            free_table_level(new_table, level);
            return NULL;
        }

        new_table[i] = (uint64_t)kernel_virt_to_phys(new_child) | (entry & 0xFFF);
    }

    return new_table;
}

uint64_t* clone_page_table(void) {
    uint64_t* new_l4 = pt_pool_alloc();
    if (!new_l4) return NULL;

    for (int i = 0; i < 512; i++) {
        if (i >= 256) {
            new_l4[i] = page_table_l4[i];
            continue;
        }

        if (!(page_table_l4[i] & PAGE_PRESENT)) {
            new_l4[i] = 0;
            continue;
        }

        uint64_t* old_l3 = (uint64_t*)phys_to_virt(page_table_l4[i] & PAGE_PHYS_MASK);
        uint64_t* new_l3 = clone_table_level(old_l3, 3);
        if (!new_l3) {
            free_page_table(new_l4);
            return NULL;
        }

        new_l4[i] = (uint64_t)kernel_virt_to_phys(new_l3) | (page_table_l4[i] & 0xFFF);
    }

    return new_l4;
}

int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, uint64_t flags) {
    // TODO: pt_pool runs out veryyyyyy quickly so it would be 
    //       wise to stop using it

    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;
    uint64_t l1_idx = (virt >> 12) & 0x1FF;

    if (!(l4_table[l4_idx] & PAGE_PRESENT)) {
        uint64_t* new_l3 = pt_pool_alloc();
        if (!new_l3) return -1;
        l4_table[l4_idx] = (uint64_t)kernel_virt_to_phys(new_l3) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        l4_table[l4_idx] |= (flags & (PAGE_RW | PAGE_USER));
    }
    uint64_t* l3 = (uint64_t*)phys_to_virt(l4_table[l4_idx] & PAGE_PHYS_MASK);

    if (!(l3[l3_idx] & PAGE_PRESENT)) {
        uint64_t* new_l2 = pt_pool_alloc();
        if (!new_l2) return -1;
        l3[l3_idx] = (uint64_t)kernel_virt_to_phys(new_l2) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        l3[l3_idx] |= (flags & (PAGE_RW | PAGE_USER));
    }

    if (l3[l3_idx] & PAGE_HUGE) {
        return -1;
    }

    uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & PAGE_PHYS_MASK);

    if (l2[l2_idx] & PAGE_HUGE) {
        uint64_t huge_entry = l2[l2_idx];
        uint64_t huge_phys_base = huge_entry & ~0x1FFFFFULL;
        uint64_t huge_flags = huge_entry & 0xFFF & ~PAGE_HUGE; 
    
        uint64_t* new_l1 = pt_pool_alloc();
        if (!new_l1) return -1;
    
        for (int i = 0; i < 512; i++) {
            new_l1[i] = (huge_phys_base + i * 0x1000) | huge_flags;
        }
    
        l2[l2_idx] = (uint64_t)kernel_virt_to_phys(new_l1) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    
        uint64_t region_base = virt & ~0x1FFFFFULL;
        for (uint64_t off = 0; off < 0x200000; off += 0x1000) {
            __asm__ volatile("invlpg (%0)" :: "r"(region_base + off) : "memory");
        }
    }

    if (!(l2[l2_idx] & PAGE_PRESENT)) {
        uint64_t* new_l1 = pt_pool_alloc();
        if (!new_l1) return -1;
        l2[l2_idx] = (uint64_t)kernel_virt_to_phys(new_l1) | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    } else {
        l2[l2_idx] |= (flags & (PAGE_RW | PAGE_USER));
    }
    uint64_t* l1 = (uint64_t*)phys_to_virt(l2[l2_idx] & PAGE_PHYS_MASK);

    l1[l1_idx] = (phys & PAGE_PHYS_MASK) | PAGE_PRESENT | flags;

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
    return 0;
}
