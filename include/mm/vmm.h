#pragma once

#include <types.h>

#include <arch/memory.h>

int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, vm_flags_t flags);
int protect_page_4k(uint64_t* l4_table, uint64_t virt, vm_flags_t flags);
int unmap_page_4k(uint64_t* l4_table, uint64_t virt);

void free_page_table(uint64_t* l4_table);
uint64_t* clone_page_table(void);

typedef struct {
    void** pages;
    size_t count;
    size_t cap;
} vm_page_list_t;

int  vm_page_list_add(vm_page_list_t* list, void* page);
void vm_page_list_free(vm_page_list_t* list);

int vm_collect_user_pages(uint64_t* l4, uint64_t skip_lo, uint64_t skip_hi, vm_page_list_t* out);

void* vm_user_to_kernel(uint64_t* l4, uint64_t va);

uint64_t* clone_user_address_space(uint64_t* src_l4, uint64_t skip_lo, uint64_t skip_hi, vm_page_list_t* owned);
void set_page_permissions(uint64_t virt, vm_flags_t flags);

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} memory_map_entry_t;

int map_physical_range(uint64_t phys_start, uint64_t size, vm_flags_t flags);
