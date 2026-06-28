#pragma once 

#include <types.h>
#include <multiboot.h>

extern void* memcpy(void* dest, const void* src, size_t n);
extern int memcmp(const void* a, const void* b, size_t n);
extern void* memset(void* dest, int val, size_t n);
extern void* phys_to_virt(uint64_t phys);

extern void memory_init(multiboot_info_t* mbd);
extern void* malloc(size_t size);
extern void free(void* ptr);

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_RW      (1ULL << 1)
#define PAGE_USER    (1ULL << 2)
#define PAGE_HUGE    (1ULL << 7)
#define PAGE_NX (1ULL << 63)

extern void set_page_permissions(uint64_t virt, uint64_t flags);
extern int map_physical_range(uint64_t phys_start, uint64_t size, uint64_t flags);
extern void set_page_executable(uint64_t virt, bool executable);
extern uint64_t* clone_page_table(void);
extern void free_page_table(uint64_t* l4);
