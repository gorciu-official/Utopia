#pragma once 

#include <types.h>

extern void* memcpy(void* dest, const void* src, size_t n);
extern int memcmp(const void* a, const void* b, size_t n);
extern void* phys_to_virt(uint64_t phys);
