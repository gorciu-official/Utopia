#include <types.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <memory.h>
#include <config.h>

#ifdef CONFIG_SMP

#include "sbi.h"

// the Limine bootloader's source code was very useful while adding SMP support for RISC-V 64:
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/smp.c
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/sbi.asm_riscv64
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/sbi.h

uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];
uint8_t kernel_stacks[CPU_ARCH_MAX_CPUS][16384] __attribute__((aligned(16)));
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

bool smp_boot_ap(long hart, uint32_t cpu_idx) {
    static smp_passed_data_t passed_data;

    cpu_t* cpu = &cpus[cpu_idx];
    cpu->id = cpu_idx;
    cpu->default_stack_top = (uintptr_t)&kernel_stacks[cpu_idx][sizeof(kernel_stacks[cpu_idx])];
    cpu->kernel_stack_top = cpu->default_stack_top;

    passed_data.stack_pointer = cpu->default_stack_top;
    passed_data.alive_table_entry_pointer = (uintptr_t)&ap_alive_table[cpu_idx];
    passed_data.page_table = smp_satp();
    passed_data.cpu_struct_pointer = (uintptr_t)cpu;

    sbicall_result_t call = sbicall(
        SBI_EID_HSM, SBI_HSM_HART_START, hart,
        kernel_virt_to_phys(smp_startup), kernel_virt_to_phys(&passed_data)
    );

    if (call.error == -6) {
        printk(KMT_LOG, "SMP", "Hart %d is already online, skipping", (int)hart);
        return false;
    } else if (call.error != 0) {
        printk(KMT_LOG, "SMP", "Could not start hart %d, error %d", (int)hart, (int)call.error);
        return false;
    }

    for (int i = 0; i < 1000000; i++) {
        if (__atomic_load_n(&ap_alive_table[cpu_idx], __ATOMIC_ACQUIRE) == 1) {
            printk(KMT_LOG, "SMP", "Hart %d is up (cpu %d).", (int)hart, (int)cpu_idx);
            return true;
        }
    }

    printk(KMT_LOG, "SMP", "Hart %d failed to register in time, considered offline.", (int)hart);

    return false;
}

void arch_boot_aps() {
    printk(KMT_LOG, "SMP", "Starting non-BSP hardware threads...");

    int online = 0;
    uint32_t next_cpu = 1;

    for (long hart = 0; hart < CPU_ARCH_MAX_CPUS && next_cpu < CPU_ARCH_MAX_CPUS; hart++) {
        sbicall_result_t status = sbicall(SBI_EID_HSM, SBI_HSM_HART_GET_STATUS, hart);

        if (status.error != 0 || status.value != SBI_HSM_STATE_STOPPED)
            continue;

        online += smp_boot_ap(hart, next_cpu);
        next_cpu++;
    }

    printk(KMT_LOG, "SMP", "Finished, %d harts are online.", online);
}

#endif
