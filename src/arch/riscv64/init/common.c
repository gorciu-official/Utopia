#include <types.h>
#include <lib/screen.h>
#include <arch/common.h>

extern void arch_init_interrupts();
extern void timer_init();

extern cpu_t cpus[CPU_ARCH_MAX_CPUS];
extern uint8_t kernel_stacks[CPU_ARCH_MAX_CPUS][16384];

void arch_early_init() {
    cpu_t* cpu = &cpus[0];

    cpu->id = 0;
    cpu->kernel_stack_top = (uintptr_t)&kernel_stacks[0][sizeof(kernel_stacks[0])];

    asm volatile(
        "mv tp, %0"
        :
        : "r"(cpu)
        : "memory"
    );

    asm volatile("csrw sscratch, zero");

    arch_init_interrupts();
    timer_init();
}

void arch_general_init() {

}

void arch_late_init() {

}

void arch_ap_init() {
    arch_init_interrupts();
    timer_init();
}
