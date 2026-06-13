#include <types.h>
#include <drivers/memory.h>
#include <drivers/screen.h>

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

    size = (size + 7) & ~7;

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
            return (void*)((uint8_t*)curr + sizeof(heap_block_t));
        }
        curr = curr->next;
    }
    return NULL;
}

void free(void* ptr) {
    if (!ptr) return;

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
}

extern uint64_t page_table_l4[];

void set_page_permissions(uint64_t virt, uint64_t flags) {
    uint64_t l4_idx = (virt >> 39) & 0x1FF;
    uint64_t l3_idx = (virt >> 30) & 0x1FF;
    uint64_t l2_idx = (virt >> 21) & 0x1FF;

    uint64_t* l3 = (uint64_t*)(page_table_l4[l4_idx] & ~0xFFF);
    if (!l3) return;
    
    uint64_t* l2 = (uint64_t*)(l3[l3_idx] & ~0xFFF);
    if (!l2) return;

    l2[l2_idx] = (l2[l2_idx] & ~0xFFF) | flags | PAGE_HUGE;
    
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
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
