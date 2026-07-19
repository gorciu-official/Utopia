#include <types.h>
#include <constants.h>
#include <memory.h>
#include <lib/screen.h>
#include <arch/x86_64/common.h>

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
#include <boot/limine.h>
#endif

extern uint8_t _end;

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};
#endif

typedef struct heap_block {
    size_t size;
    struct heap_block* next;
    bool free;
} heap_block_t;

static heap_block_t* head = NULL;
static memory_map_entry_t kernel_memory_map[64];
static uint32_t memory_map_count = 0;

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
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
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
void memory_init(void) {
    vmm_init();

    struct limine_memmap_response* response = memmap_request.response;
    if (response == NULL) {
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

    uint64_t largest_free_base = 0;
    uint64_t largest_free_size = 0;

    memory_map_count = 0;
    for (uint64_t i = 0; i < response->entry_count; i++) {
        struct limine_memmap_entry* entry = response->entries[i];
        if (memory_map_count < 64) {
            kernel_memory_map[memory_map_count].addr = entry->base;
            kernel_memory_map[memory_map_count].len = entry->length;

            uint32_t type = 2; // default to reserved lol
            if (entry->type == LIMINE_MEMMAP_USABLE) {
                type = 1;
            } else if (entry->type == LIMINE_MEMMAP_RESERVED) {
                type = 2;
            } else if (entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE) {
                type = 3;
            } else if (entry->type == LIMINE_MEMMAP_ACPI_NVS) {
                type = 4;
            } else if (entry->type == LIMINE_MEMMAP_BAD_MEMORY) {
                type = 5;
            }
            kernel_memory_map[memory_map_count].type = type;
            memory_map_count++;
        }

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (entry->length > largest_free_size) {
                largest_free_base = entry->base;
                largest_free_size = entry->length;
            }
        }
    }
#endif

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
    uintptr_t kernel_end_phys = kernel_virt_to_phys((void*)kernel_end);

    if (largest_free_base < kernel_end_phys && largest_free_base + largest_free_size > kernel_end_phys) {
        largest_free_size -= (kernel_end_phys - largest_free_base);
        largest_free_base = kernel_end_phys;
    }

    head = (heap_block_t*)phys_to_virt(largest_free_base);
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

    void pmm_init(); // forward declaration
    pmm_init();
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

#define PAGE_SIZE 4096ULL

static uint8_t* pmm_bitmap = NULL;
static uint64_t pmm_bitmap_bytes = 0;
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_search_hint = 0;

static inline void bitmap_set(uint64_t page) {
    pmm_bitmap[page >> 3] |= (1u << (page & 7));
}
static inline void bitmap_clear(uint64_t page) {
    pmm_bitmap[page >> 3] &= ~(1u << (page & 7));
}
static inline bool bitmap_test(uint64_t page) {
    return pmm_bitmap[page >> 3] & (1u << (page & 7));
}

static void pmm_mark_range_used(uint64_t phys_start, uint64_t len) {
    uint64_t start_page = phys_start / PAGE_SIZE;
    uint64_t end_page = (phys_start + len + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t p = start_page; p < end_page && p < pmm_total_pages; p++) {
        bitmap_set(p);
    }
}

static void pmm_mark_range_free(uint64_t phys_start, uint64_t len) {
    uint64_t start_page = (phys_start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t end_page = (phys_start + len) / PAGE_SIZE;
    for (uint64_t p = start_page; p < end_page && p < pmm_total_pages; p++) {
        bitmap_clear(p);
    }
}

void pmm_init(void) {
    uint64_t highest = 0;
    for (uint32_t i = 0; i < memory_map_count; i++) {
        uint64_t end = kernel_memory_map[i].addr + kernel_memory_map[i].len;
        if (end > highest) highest = end;
    }

    pmm_total_pages = (highest + PAGE_SIZE - 1) / PAGE_SIZE;
    pmm_bitmap_bytes = (pmm_total_pages + 7) / 8;

    pmm_bitmap = (uint8_t*)malloc(pmm_bitmap_bytes);
    if (!pmm_bitmap) {
        printk("PMM", "Failed to allocate %lu byte bitmap", pmm_bitmap_bytes);
        return;
    }
    memset(pmm_bitmap, 0xFF, pmm_bitmap_bytes);

    for (uint32_t i = 0; i < memory_map_count; i++) {
        if (kernel_memory_map[i].type != 1) continue; 
        pmm_mark_range_free(kernel_memory_map[i].addr, kernel_memory_map[i].len);
    }

    pmm_mark_range_used(kernel_virt_to_phys((void*)head), head->size + sizeof(heap_block_t));
}

uint64_t pmm_alloc_page(void) {
    if (!pmm_bitmap) return 0;

    uint64_t flags = save_interrupts();

    uint64_t result = 0;
    for (uint64_t i = 0; i < pmm_total_pages; i++) {
        uint64_t page = (pmm_search_hint + i) % pmm_total_pages;
        if (!bitmap_test(page)) {
            bitmap_set(page);
            pmm_search_hint = page + 1;
            result = page * PAGE_SIZE;
            break;
        }
    }

    restore_interrupts(flags);

    if (!result) {
        printk("PMM", "Out of physical memory");
    }
    return result;
}

void pmm_free_page(uint64_t phys_addr) {
    if (!phys_addr || !pmm_bitmap) return;

    uint64_t page = phys_addr / PAGE_SIZE;
    if (page >= pmm_total_pages) return;

    uint64_t flags = save_interrupts();

    if (!bitmap_test(page)) {
        printk("PMM", "Warning: double free of page %p", (void*)phys_addr);
    } else {
        bitmap_clear(page);
        if (page < pmm_search_hint) pmm_search_hint = page;
    }

    restore_interrupts(flags);
}
