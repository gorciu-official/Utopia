#pragma once 

#include <types.h>
#include <multiboot.h>

extern void* memcpy(void* dest, const void* src, size_t n);
extern int memcmp(const void* a, const void* b, size_t n);
extern void* phys_to_virt(uint64_t phys);

void memory_init(multiboot_info_t* mbd);
void* malloc(size_t size);
void free(void* ptr);

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_RW      (1ULL << 1)
#define PAGE_USER    (1ULL << 2)
#define PAGE_HUGE    (1ULL << 7)

void set_page_permissions(uint64_t virt, uint64_t flags);
