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
    page_table_l4 = (uint64_t*)phys_to_virt(cr3_phys & ~0xFFFULL);
#endif
}

#define PT_POOL_PAGES 64
static uint8_t pt_pool[PT_POOL_PAGES][4096] __attribute__((aligned(4096)));
static uint32_t pt_pool_next = 0;

static uint64_t* pt_pool_alloc(void) {
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

        uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & ~0xFFFULL);

        if (!(l3[l3_idx] & PAGE_PRESENT)) {
            uint64_t* new_l2 = pt_pool_alloc();
            if (!new_l2) return -1;
            l3[l3_idx] = (uint64_t)kernel_virt_to_phys(new_l2) | PAGE_PRESENT | PAGE_RW;
        }

        uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & ~0xFFFULL);

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
    uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & ~0xFFFULL);

    if (!(l3[l3_idx] & PAGE_PRESENT)) return;
    l3[l3_idx] |= (flags & (PAGE_USER | PAGE_RW));
    uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & ~0xFFFULL);

    if (l2[l2_idx] & PAGE_HUGE) {
        l2[l2_idx] = (l2[l2_idx] & ~0xFFFULL) | flags | PAGE_HUGE;
    } else if (l2[l2_idx] & PAGE_PRESENT) {
        l2[l2_idx] |= (flags & (PAGE_USER | PAGE_RW));
        uint64_t l1_idx = (virt >> 12) & 0x1FF;
        uint64_t* l1 = (uint64_t*)phys_to_virt(l2[l2_idx] & ~0xFFFULL);
        l1[l1_idx] = (l1[l1_idx] & ~0xFFFULL) | flags;
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void set_page_executable(uint64_t virt, bool executable) {
    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;

    if (!(page_table_l4[l4_idx] & PAGE_PRESENT))
        return;

    uint64_t* l3 = (uint64_t*)phys_to_virt(page_table_l4[l4_idx] & ~0xFFFULL);

    if (!(l3[l3_idx] & PAGE_PRESENT))
        return;

    uint64_t* l2 = (uint64_t*)phys_to_virt(l3[l3_idx] & ~0xFFFULL);

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
    uint64_t* l1 = (uint64_t*)phys_to_virt(l2[l2_idx] & ~0xFFFULL);

    if (!(l2[l2_idx] & PAGE_PRESENT))
        return;

    if (executable) {
        l1[l1_idx] &= ~PAGE_NX;
    } else {
        l1[l1_idx] |= PAGE_NX;
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

uint64_t* clone_page_table(void) {
    uint64_t* new_l4 = pt_pool_alloc();
    if (!new_l4) return NULL;

    for (int i = 0; i < 512; i++) {
        new_l4[i] = page_table_l4[i];
    }

    return new_l4;
}

void free_page_table(uint64_t* l4) {
    if (!l4 || l4 == page_table_l4) return;
    memset(l4, 0, 4096);
}
