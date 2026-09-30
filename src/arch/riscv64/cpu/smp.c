#include <types.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <memory.h>

#include "sbi.h"

// the Limine bootloader's source code was very useful while adding SMP support for RISC-V 64:
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/smp.c
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/sbi.asm_riscv64
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/sbi.h

uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];
uint8_t kernel_stacks[CPU_ARCH_MAX_CPUS][16384];
cpu_t cpus[CPU_ARCH_MAX_CPUS];

typedef struct {
    uint64_t /* offset 0  */ stack_pointer; 
    uint64_t /* offset 8  */ alive_table_entry_pointer;
    uint64_t /* offset 16 */ page_table;
    uint64_t /* offset 24 */ cpu_struct_pointer;
} __attribute__((packed)) smp_passed_data_t;

__attribute__((naked, used, aligned(4)))
void smp_startup(void) {
    asm volatile (
        "ld sp,  0(a1)\n"
        "ld t0,  8(a1)\n"   
        "ld t1, 16(a1)\n"  
        "ld tp, 24(a1)\n" 

        "csrw satp, t1\n"
        "sfence.vma\n"

        "csrw sscratch, zero\n"

        "li t1, 1\n"
        "sb t1, 0(t0)\n"

        "j ap_main\n"
    );
}

uintptr_t smp_satp(void) {
    uint64_t satp;
    asm volatile("csrr %0, satp" : "=r"(satp));
    return satp;
}

bool smp_boot_ap(int hart) {
    static smp_passed_data_t passed_data;
    
    cpu_t* cpu = &cpus[hart + 1];
    cpu->id = hart + 1;
    cpu->kernel_stack_top = (uintptr_t)&kernel_stacks[hart + 1][sizeof(kernel_stacks[hart + 1])];

    passed_data.stack_pointer = (uintptr_t)&kernel_stacks[hart + 1][sizeof(kernel_stacks[hart + 1])];
    passed_data.alive_table_entry_pointer = (uintptr_t)&ap_alive_table[hart + 1];
    passed_data.page_table = smp_satp();
    passed_data.cpu_struct_pointer = (uintptr_t)cpu;

    sbicall_result_t call = sbicall(
        SBI_EID_HSM, 0, hart, 
        kernel_virt_to_phys(smp_startup), kernel_virt_to_phys(&passed_data)
    );

    if (call.error == -6) {
        printk("SMP", "Hart %d is allegedly already online", hart);
        return true;
    } else if (call.error != 0) {
        printk("SMP", "Could not start hart %d, error %d", hart, call.error);
        return false;
    }

    for (int i = 0; i < 1000000; i++) {
        if (ap_alive_table[hart + 1] == 1) {
            printk("SMP", "Hart %d is up.", hart);
            return true;
        }
    }

    printk("SMP", "Hart %d failed to register in time, considered offline.", hart);

    return false;
}

void arch_boot_aps() {
    printk("SMP", "Starting non-BSP hardware threads...");

    int online = 0;

    for (int hart_id = 0; hart_id < 4; hart_id++) { // TODO: do not hardcode lol
        online += smp_boot_ap(hart_id); // assuming 1 is true
    }

    printk("SMP", "Finished, %d harts are online.", online);
}
