#include <types.h>
#include <arch/common.h>

uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

extern void arch_init_interrupts();

void arch_early_init() {
    arch_init_interrupts();
}

void arch_general_init() {

}

void arch_late_init() {

}

void arch_boot_aps() {

}

void arch_ap_init() {

}
