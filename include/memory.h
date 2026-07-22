#pragma once 

#include <types.h>
#include <constants.h>

void vmm_init(void);

void* phys_to_virt(uint64_t phys);
uintptr_t kernel_virt_to_phys(void* addr);

int map_physical_range(uint64_t phys_start, uint64_t size, uint64_t flags);
int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, uint64_t flags);
void set_page_permissions(uint64_t virt, uint64_t flags);
void set_page_executable(uint64_t virt, bool executable);
uint64_t* clone_page_table(void);
void free_page_table(uint64_t* l4);

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int val, size_t n);
int memcmp(const void* a, const void* b, size_t n);
void* memmove(void *dst, const void *src, size_t n);

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} memory_map_entry_t;

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
#include <boot/multiboot1.h>

void memory_init(multiboot_info_t* mbd);
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
void memory_init(void);
#endif

void* malloc(size_t size);
void free(void* ptr);

#define PAGE_PRESENT       (1ULL << 0)
#define PAGE_RW            (1ULL << 1)
#define PAGE_USER          (1ULL << 2)
#define PAGE_HUGE          (1ULL << 7)
#define PAGE_NX            (1ULL << 63)
#define PAGE_PHYS_MASK     0x000FFFFFFFFFF000ULL

void pmm_free_page(uint64_t phys_addr);
uint64_t pmm_alloc_page(void);
