#pragma once 

#include <types.h>

void vmm_init(void);
int memory_reserve_range(uint64_t reserve_start, uint64_t reserve_end);

void* malloc(size_t size);
void* calloc(size_t count, size_t size);
void free(void* ptr);

void* phys_to_virt(uint64_t phys);
uintptr_t hhdm_virt_to_phys(void* addr);
uintptr_t kernel_virt_to_phys(void* addr);

uintptr_t kernel_page_table_phys(void);

void* page_alloc(uint64_t pages);
uint64_t* pt_pool_alloc(void);

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
extern uint64_t* page_table_l4;
#else 
extern uint64_t page_table_l4[];
#endif

extern void* memset(void* dest, int val, size_t n);
extern void* memcpy(void* dest, const void* src, size_t n);
extern int memcmp(const void* a, const void* b, size_t n);
extern void* memmove(void *dst, const void *src, size_t n);

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
#include <boot/multiboot1.h>
void memory_init_base(multiboot_info_t* mbd);
#else
void memory_init_base();
#endif
void memory_init();
