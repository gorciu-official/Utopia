#pragma once 

#include <types.h>
#include <constants.h>
#include <arch/memory.h>

void vmm_init(void);
int memory_reserve_range(uint64_t reserve_start, uint64_t reserve_end);

void* malloc(size_t size);
void free(void* ptr);

void* phys_to_virt(uint64_t phys);
uintptr_t hhdm_virt_to_phys(void* addr);
uintptr_t kernel_virt_to_phys(void* addr);

void* page_alloc(uint64_t pages);
uint64_t* pt_pool_alloc(void);

void* memset(void* dest, int val, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* a, const void* b, size_t n);
void* memmove(void *dst, const void *src, size_t n);

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
#include <boot/multiboot1.h>
void memory_init_base(multiboot_info_t* mbd);
#else
void memory_init_base();
#endif
void memory_init();

int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, vm_flags_t flags);
int protect_page_4k(uint64_t* l4_table, uint64_t virt, vm_flags_t flags);
int unmap_page_4k(uint64_t* l4_table, uint64_t virt);

void free_page_table(uint64_t* l4_table);
uint64_t* clone_page_table(void);
void set_page_permissions(uint64_t virt, vm_flags_t flags);

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} memory_map_entry_t;

int map_physical_range(uint64_t phys_start, uint64_t size, vm_flags_t flags);
