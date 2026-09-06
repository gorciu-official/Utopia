#include <types.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <memory.h>

// this thing was useful for SMP implementation:
//   https://github.com/Limine-Bootloader/Limine/blob/v12.x/common/sys/smp.c

uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

typedef struct {
    long error;
    long value;
} sbicall_result_t;

extern sbicall_result_t sbicall(int eid, int fid, ...);

#define SBI_EID_HSM  0x48534d

__attribute__((naked, used, aligned(4)))
void smp_startup(void) {
    asm volatile (
        "li t0, 1\n"
        "sb t0, 0(a1)\n"
        "1:\n"
        "wfi\n"
        "j 1b\n"
    );
}

bool smp_boot_ap(int hart) {
    sbicall_result_t call = sbicall(
        SBI_EID_HSM, 0, hart, 
        kernel_virt_to_phys(smp_startup), kernel_virt_to_phys(&ap_alive_table[hart])
    );

    if (call.error == -6) {
        printk("SMP", "Hart %d is allegedly already online", hart);
        return true;
    } else if (call.error != 0) {
        printk("SMP", "Could not start hart %d, error %d", hart, call.error);
        return false;
    }

    for (int i = 0; i < 1000000; i++) {
        if (ap_alive_table[hart] == 1) {
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
