#include <drivers/cpu.h>
#include <spinlock.h>
#include <string.h>
#include <types.h>
#include <drivers/memory.h>
#include <drivers/screen.h>

#define MAX_CPUS          CPU_ARCH_MAX_CPUS

#define KERNEL_CODE_SEL   0x08  
#define KERNEL_DATA_SEL   0x10  
#define USER_CODE32_SEL   0x18 
#define USER_DATA_SEL     0x20  
#define USER_CODE_SEL     0x28  
#define TSS_SEL           0x30 

static gdt_slot_t gdt_per_cpu[MAX_CPUS][8];
static gdt_ptr_t  gdt_ptr_per_cpu[MAX_CPUS];
static tss_t      tss_per_cpu[MAX_CPUS];
static spinlock_t gdt_spinlock;

typedef struct {
    uint64_t kernel_stack;
    uint64_t unused;
    uint64_t user_stack;
} __attribute__((packed)) cpu_local_t;

static cpu_local_t cpu_local_data[MAX_CPUS];
uint8_t kernel_stacks[MAX_CPUS][16384] __attribute__((aligned(16)));

extern void gdt_flush(uint64_t);

void tss_set_rsp0(int cpu_id, uint64_t stack_top) {
    if (cpu_id < MAX_CPUS) {
        tss_per_cpu[cpu_id].rsp0 = stack_top;
    }
}

static void tss_init_per_cpu(int cpu_id) {
    memset(&tss_per_cpu[cpu_id], 0, sizeof(tss_t));

    uint64_t top = (uint64_t)&kernel_stacks[cpu_id][16384];

    tss_per_cpu[cpu_id].rsp0 = top;
    tss_per_cpu[cpu_id].iomap_base = sizeof(tss_t); 
}

static void gdt_set_entry(gdt_slot_t *gdt_target, int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_target[idx].fields.limit_low   = limit & 0xFFFF;
    gdt_target[idx].fields.base_low    = base & 0xFFFF;
    gdt_target[idx].fields.base_middle = (base >> 16) & 0xFF;
    gdt_target[idx].fields.access      = access;
    gdt_target[idx].fields.granularity = (limit >> 16) & 0x0F;
    gdt_target[idx].fields.granularity |= gran & 0xF0;
    gdt_target[idx].fields.base_high   = (base >> 24) & 0xFF;
}

static void gdt_set_tss(gdt_slot_t *gdt_target, int idx, uint64_t base, uint32_t limit) {
    uint8_t access = 0x89; 
    gdt_set_entry(gdt_target, idx, (uint32_t)base, limit, access, 0x00);
    
    uint32_t base_high32 = (uint32_t)(base >> 32);
    gdt_target[idx + 1].raw = (uint64_t)base_high32;
}

void gdt_init() {
    int cpu_id = current_processor_id();
    if (cpu_id >= MAX_CPUS) {
        return;    
    }

    spinlock_acquire(&gdt_spinlock);

    tss_init_per_cpu(cpu_id);

    gdt_slot_t *my_gdt = gdt_per_cpu[cpu_id];
    gdt_ptr_per_cpu[cpu_id].limit = (sizeof(gdt_slot_t) * 8) - 1;
    gdt_ptr_per_cpu[cpu_id].base  = (uint64_t)my_gdt;

    gdt_set_entry(my_gdt, 0, 0, 0, 0, 0);
    gdt_set_entry(my_gdt, 1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    gdt_set_entry(my_gdt, 2, 0, 0xFFFFFFFF, 0x92, 0xC0); 
    gdt_set_entry(my_gdt, 3, 0, 0xFFFFFFFF, 0xFA, 0xA0); 
    gdt_set_entry(my_gdt, 4, 0, 0xFFFFFFFF, 0xF2, 0xC0); 
    gdt_set_entry(my_gdt, 5, 0, 0xFFFFFFFF, 0xFA, 0xA0); 
    gdt_set_tss(my_gdt, 6, (uint64_t)&tss_per_cpu[cpu_id], sizeof(tss_t) - 1);
    gdt_flush((uint64_t)&gdt_ptr_per_cpu[cpu_id]);

    asm volatile("mfence" ::: "memory");
    asm volatile("ltr %0" :: "r"((uint16_t)TSS_SEL));

    cpu_local_data[cpu_id].kernel_stack = (uint64_t)&kernel_stacks[cpu_id][16384];
    cpu_local_data[cpu_id].user_stack = 0;

    write_msr(0xC0000101, (uint64_t)&cpu_local_data[cpu_id]); // GS_BASE
    write_msr(0xC0000102, (uint64_t)&cpu_local_data[cpu_id]); // KERNEL_GS_BASE
    
    spinlock_release(&gdt_spinlock);
}
