#include <types.h>
#include <constants.h>
#include <drivers/screen.h>
#include <drivers/cpu.h>
#include <drivers/memory.h>

void delay(uint64_t count) {
    for (volatile uint64_t i = 0; i < count; i++) {
        asm volatile("pause");
    }
}

extern uint8_t ap_start[];
extern uint8_t ap_end[];

volatile uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS] = {0}; 
extern uint8_t kernel_stacks[CPU_ARCH_MAX_CPUS][16384];
volatile uint64_t ap_stack_ptr = 0;

extern void ap_main();

void boot_ap(uint8_t target_apic_id) {
uint8_t* trampoline_dest = (uint8_t*)0x70000;
    size_t trampoline_size = (size_t)(ap_end - ap_start);

    memcpy(trampoline_dest, ap_start, trampoline_size);

    volatile uint32_t* ap_cr3   = (volatile uint32_t*)0x70F00;
    volatile uint64_t* ap_gdt   = (volatile uint64_t*)0x70F08;
    volatile uint64_t* ap_main_f = (volatile uint64_t*)0x70F10;
    volatile uint64_t* ap_stack = (volatile uint64_t*)0x70F18;

    uint64_t bsp_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(bsp_cr3));
    *ap_cr3 = (uint32_t)bsp_cr3;

    gdt_ptr_t bsp_gdt;
    asm volatile("sgdt %0" : "=m"(bsp_gdt));

    gdt_ptr_t* ap_gdt_dest = (gdt_ptr_t*)0x70F30;
    ap_gdt_dest->limit = bsp_gdt.limit;
    ap_gdt_dest->base  = bsp_gdt.base;

    *ap_gdt = (uint64_t)(uintptr_t)ap_gdt_dest;

    *ap_main_f = (uint64_t)(uintptr_t)ap_main;
    
    *ap_stack = (uint64_t)(uintptr_t)&kernel_stacks[target_apic_id][16384];

    ap_alive_table[target_apic_id] = 0;

    volatile uint32_t* lapic_high = (uint32_t*)0xFEE00310;
    volatile uint32_t* lapic_low  = (uint32_t*)0xFEE00300;
    
    *lapic_high = (target_apic_id << 24);
    *lapic_low  = 0x00004500; 
    delay(10000);

    *lapic_high = (target_apic_id << 24);
    *lapic_low  = 0x00004670; 
    delay(10000);

    *lapic_high = (target_apic_id << 24);
    *lapic_low  = 0x00004670;
    delay(10000);

    for (int timeout = 0; timeout < 100000; timeout++) {
        if (ap_alive_table[target_apic_id] == 1) {
            break;
        }
        asm volatile("pause");
    }
}

void boot_all_aps(uint8_t* core_apic_ids, int count) {
    uint32_t bsp_apic_id = current_processor_id();

    printk("SMP", "Starting all application processors (total: %d)", count - 1);

    uint32_t failed_to_boot = 0;

    for (int i = 0; i < count; i++) {
        uint8_t apic_id = core_apic_ids[i];

        if (apic_id == bsp_apic_id) {
            continue;
        }

        printk("SMP", "Waking up CPU %d...", apic_id);
        boot_ap(apic_id);

        if (ap_alive_table[apic_id] == 1) {
            printk("SMP", "CPU %d is online", apic_id);
        } else {
            printk("SMP", "CPU %d failed to register in time or is offline!", apic_id);
            failed_to_boot++;
        }
    }

    printk("SMP", "Finished - %d CPUs are up", count - failed_to_boot);
}
