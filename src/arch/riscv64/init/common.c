#include <types.h>
#include <arch/common.h>

extern void arch_init_interrupts();

void arch_early_init() {
    arch_init_interrupts();
}

void arch_general_init() {

}

void arch_late_init() {

}

void arch_ap_init() {

}
