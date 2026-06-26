#include "spinlock.h"
#include <string.h>
#include <types.h>
#include <drivers/memory.h>
#include <drivers/screen.h>

// some of this shit was corrected by ai 
// since i couldn't figure the problem lol

#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10
#define USER_CODE_SEL   0x18
#define USER_DATA_SEL   0x20
#define TSS_SEL         0x28

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} tss_t;

typedef union {
    gdt_entry_t fields;
    uint64_t raw;
} gdt_slot_t;

static gdt_slot_t gdt[7];
static gdt_ptr_t gdt_p;
static tss_t tss;
static spinlock_t gdt_spinlock;

extern void gdt_flush(uint64_t);

void tss_set_rsp0(uint64_t stack_top) {
    tss.rsp0 = stack_top;
}

void tss_init() {
    memset(&tss, 0, sizeof(tss));

    void *stack = malloc(4096);
    uint64_t top = (uint64_t)stack + 4096;

    tss.rsp0 = top;
    tss.iomap_base = sizeof(tss); 
}

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[idx].fields.limit_low   = limit & 0xFFFF;
    gdt[idx].fields.base_low    = base & 0xFFFF;
    gdt[idx].fields.base_middle = (base >> 16) & 0xFF;
    gdt[idx].fields.access      = access;
    gdt[idx].fields.granularity = (limit >> 16) & 0x0F;
    gdt[idx].fields.granularity |= gran & 0xF0;
    gdt[idx].fields.base_high   = (base >> 24) & 0xFF;
}

static void gdt_set_tss(int idx, uint64_t base, uint32_t limit) {
    uint8_t access = 0x89;    
    gdt_set_entry(idx, (uint32_t)base, limit, access, 0x00);
    
    uint32_t base_high32 = (uint32_t)(base >> 32);
    
    gdt[idx + 1].raw = 0; 
    
    gdt[idx + 1].fields.limit_low   = (uint16_t)(base_high32 & 0xFFFF); 
    gdt[idx + 1].fields.base_low    = (uint8_t)((base_high32 >> 16) & 0xFF);
    gdt[idx + 1].fields.base_middle = (uint8_t)((base_high32 >> 24) & 0xFF);
}

void gdt_init() {
    spinlock_acquire(&gdt_spinlock);

    tss_init();

    gdt_p.limit = sizeof(gdt) - 1;
    gdt_p.base  = (uint64_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xA0);
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xA0);
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xA0);
    gdt_set_tss(5, (uint64_t)&tss, sizeof(tss) - 1);

    gdt_flush((uint64_t)&gdt_p);

    asm volatile("mfence" ::: "memory");
    asm volatile("ltr %0" :: "r"((uint16_t)TSS_SEL));
    
    spinlock_release(&gdt_spinlock);
}
