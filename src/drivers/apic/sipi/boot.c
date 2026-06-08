#include <types.h>
#include <drivers/screen.h>
#include <drivers/cpu.h>

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void delay(uint64_t count) {
    for (volatile uint64_t i = 0; i < count; i++) {
        asm volatile("pause");
    }
}

extern uint8_t ap_start[];
extern uint8_t ap_end[];
extern uint32_t ap_alive_table_ptr;

volatile uint8_t ap_alive_table[256] = {0}; 

void boot_ap(uint8_t target_apic_id) {
    uint8_t* trampoline_dest = (uint8_t*)0x70000;
    size_t trampoline_size = (size_t)(ap_end - ap_start);

    memcpy(trampoline_dest, ap_start, trampoline_size);

    uintptr_t patch_offset = (uintptr_t)&ap_alive_table_ptr - (uintptr_t)ap_start;
    uint32_t* table_ptr_in_trampoline = (uint32_t*)(trampoline_dest + patch_offset);
    *table_ptr_in_trampoline = (uint32_t)(uintptr_t)&ap_alive_table;

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

    for (int timeout = 0; timeout < 10000; timeout++) {
        if (ap_alive_table[target_apic_id] == 1) {
            break;
        }
        asm volatile("pause");
    }
}


void boot_all_aps(uint8_t total_cores) {
    uint32_t bsp_apic_id = current_processor_id();

    printk("CPU manager", "Starting all application processors (total: %d)", total_cores - 1);

    for (uint8_t id = 0; id < total_cores; id++) {
        if (id == bsp_apic_id) {
            continue;
        }

        printk("CPU manager", "Waking up CPU %d...", id);
        boot_ap(id);

        if (ap_alive_table[id] == 1) {
            printk("CPU manager", "CPU %d is up", id);
        } else {
            printk("CPU manager", "CPU %d failed to start/register!", id);
        }
    }
}
