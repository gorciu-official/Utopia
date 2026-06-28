#include <types.h>
#include <drivers/memory.h>
#include <drivers/screen.h>
#include <drivers/cpu.h>

extern uint8_t _end;

typedef struct heap_block {
    size_t size;
    struct heap_block* next;
    bool free;
} heap_block_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} memory_map_entry_t;

static heap_block_t* head = NULL;
static memory_map_entry_t kernel_memory_map[64];
static uint32_t memory_map_count = 0;

void memory_init(multiboot_info_t* mbd) {
    if (!(mbd->flags & MULTIBOOT_FLAG_MMAP)) {
        head = (heap_block_t*)(((uintptr_t)&_end + 4095) & ~4095);
        head->size = (128 * 1024 * 1024) - sizeof(heap_block_t);
        head->next = NULL;
        head->free = true;

        kernel_memory_map[0].addr = (uintptr_t)head;
        kernel_memory_map[0].len = head->size;
        kernel_memory_map[0].type = 1;
        memory_map_count = 1;
        return;
    }

    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)((uintptr_t)mbd->mmap_addr);
    uint32_t mmap_length = mbd->mmap_length;

    uint64_t largest_free_base = 0;
    uint64_t largest_free_size = 0;

    memory_map_count = 0;
    for (multiboot_memory_map_t* entry = mmap; (uintptr_t)entry < mbd->mmap_addr + mmap_length; entry = (multiboot_memory_map_t*)((uintptr_t)entry + entry->size + sizeof(entry->size))) {
        if (memory_map_count < 64) {
            kernel_memory_map[memory_map_count].addr = entry->addr;
            kernel_memory_map[memory_map_count].len = entry->len;
            kernel_memory_map[memory_map_count].type = entry->type;
            memory_map_count++;
        }

        if (entry->type == 1) {
            if (entry->len > largest_free_size) {
                largest_free_base = entry->addr;
                largest_free_size = entry->len;
            }
        }
    }

    for (uint32_t i = 0; i < memory_map_count; i++) {
        if (kernel_memory_map[i].type != 1) continue;

        uint64_t region_start = kernel_memory_map[i].addr;
        uint64_t region_end   = region_start + kernel_memory_map[i].len;

        if (region_end <= 0x100000000ULL) continue;
        if (region_start < 0x100000000ULL) region_start = 0x100000000ULL;

        uint64_t map_start = region_start & ~0x1FFFFFULL;
        uint64_t map_size  = ((region_end + 0x1FFFFFULL) & ~0x1FFFFFULL) - map_start;

        if (map_physical_range(map_start, map_size, 0) != 0) {
            printk("Memory", "Warning: failure in mappping region above 4GB at %p", map_start);
        }
    }

    uintptr_t kernel_end = ((uintptr_t)&_end + 4095) & ~4095;

    if (largest_free_base < kernel_end && largest_free_base + largest_free_size > kernel_end) {
        largest_free_size -= (kernel_end - largest_free_base);
        largest_free_base = kernel_end;
    }

    head = (heap_block_t*)largest_free_base;
    head->size = largest_free_size - sizeof(heap_block_t);
    head->next = NULL;
    head->free = true;

    printk("Memory", "e820: BIOS-provided memory map (%d entries):", memory_map_count);
    for (uint32_t i = 0; i < memory_map_count; i++) {
        const char* type_str = "Unknown";
        switch (kernel_memory_map[i].type) {
            case 1: type_str = "Available"; break;
            case 2: type_str = "Reserved"; break;
            case 3: type_str = "ACPI Reclaim"; break;
            case 4: type_str = "NVS"; break;
            case 5: type_str = "Bad"; break;
        }
        printk("Memory", "e820:  [%p - %p] %s (%lu bytes)", 
            kernel_memory_map[i].addr, 
            kernel_memory_map[i].addr + kernel_memory_map[i].len,
            type_str,
            kernel_memory_map[i].len);
    }
    printk("Memory", "Heap initialized at %p with %lu bytes", head, head->size);
}

void* malloc(size_t size) {
    if (size == 0) return NULL;

    uint64_t flags = save_interrupts();

    size = (size + 7) & ~7;

    void* result = NULL;
    heap_block_t* curr = head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size >= size + sizeof(heap_block_t) + 16) {
                heap_block_t* next = (heap_block_t*)((uint8_t*)curr + sizeof(heap_block_t) + size);
                next->size = curr->size - size - sizeof(heap_block_t);
                next->next = curr->next;
                next->free = true;

                curr->size = size;
                curr->next = next;
            }
            curr->free = false;
            result = (void*)((uint8_t*)curr + sizeof(heap_block_t));
            break;
        }
        curr = curr->next;
    }

    restore_interrupts(flags);
    return result;
}

void free(void* ptr) {
    if (!ptr) return;

    uint64_t flags = save_interrupts();

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->free = true;

    heap_block_t* curr = head;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            curr->size += sizeof(heap_block_t) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }

    restore_interrupts(flags);
}

extern uint64_t page_table_l4[];

#define PT_POOL_PAGES 64
static uint8_t pt_pool[PT_POOL_PAGES][4096] __attribute__((aligned(4096)));
static uint32_t pt_pool_next = 0;

static uint64_t* pt_pool_alloc(void) {
    if (pt_pool_next >= PT_POOL_PAGES) return NULL;
    uint64_t* page = (uint64_t*)pt_pool[pt_pool_next++];
    memset(page, 0, 4096);
    return page;
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
            page_table_l4[l4_idx] = (uint64_t)(uintptr_t)new_l3 | PAGE_PRESENT | PAGE_RW;
        }

        uint64_t* l3 = (uint64_t*)(uintptr_t)(page_table_l4[l4_idx] & ~0xFFFULL);

        if (!(l3[l3_idx] & PAGE_PRESENT)) {
            uint64_t* new_l2 = pt_pool_alloc();
            if (!new_l2) return -1;
            l3[l3_idx] = (uint64_t)(uintptr_t)new_l2 | PAGE_PRESENT | PAGE_RW;
        }

        uint64_t* l2 = (uint64_t*)(uintptr_t)(l3[l3_idx] & ~0xFFFULL);

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
    uint64_t* l3 = (uint64_t*)(uintptr_t)(page_table_l4[l4_idx] & ~0xFFFULL);

    if (!(l3[l3_idx] & PAGE_PRESENT)) return;
    l3[l3_idx] |= (flags & (PAGE_USER | PAGE_RW));
    uint64_t* l2 = (uint64_t*)(uintptr_t)(l3[l3_idx] & ~0xFFFULL);

    if (l2[l2_idx] & PAGE_HUGE) {
        l2[l2_idx] = (l2[l2_idx] & ~0xFFFULL) | flags | PAGE_HUGE;
    } else if (l2[l2_idx] & PAGE_PRESENT) {
        l2[l2_idx] |= (flags & (PAGE_USER | PAGE_RW));
        uint64_t l1_idx = (virt >> 12) & 0x1FF;
        uint64_t* l1 = (uint64_t*)(uintptr_t)(l2[l2_idx] & ~0xFFFULL);
        l1[l1_idx] = (l1[l1_idx] & ~0xFFFULL) | flags;
    }

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint64_t* d64 = (uint64_t*)dest;
    const uint64_t* s64 = (const uint64_t*)src;
    size_t n64 = n / 8;
    for (size_t i = 0; i < n64; i++) {
        d64[i] = s64[i];
    }
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = n64 * 8; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = a;
    const uint8_t* y = b;

    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i]) return 1;

    return 0;
}

void* phys_to_virt(uint64_t phys) {
    return (void*)phys;
}

void* memset(void* dest, int val, size_t n) {
    uint64_t v64 = (uint8_t)val;
    v64 |= v64 << 8;
    v64 |= v64 << 16;
    v64 |= v64 << 32;

    uint64_t* d64 = (uint64_t*)dest;
    size_t n64 = n / 8;
    for (size_t i = 0; i < n64; i++) {
        d64[i] = v64;
    }
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = n64 * 8; i < n; i++) {
        d[i] = (uint8_t)val;
    }
    return dest;
}

void set_page_executable(uint64_t virt, bool executable) {
    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;

    if (!(page_table_l4[l4_idx] & PAGE_PRESENT))
        return;

    uint64_t* l3 = (uint64_t*)(uintptr_t)(page_table_l4[l4_idx] & ~0xFFFULL);

    if (!(l3[l3_idx] & PAGE_PRESENT))
        return;

    uint64_t* l2 = (uint64_t*)(uintptr_t)(l3[l3_idx] & ~0xFFFULL);

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
    uint64_t* l1 = (uint64_t*)(uintptr_t)(l2[l2_idx] & ~0xFFFULL);

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
