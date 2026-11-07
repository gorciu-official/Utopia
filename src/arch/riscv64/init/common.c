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
    cpu->default_stack_top = (uintptr_t)&kernel_stacks[0][sizeof(kernel_stacks[0])];
    cpu->kernel_stack_top = cpu->default_stack_top;

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

void arch_set_kernel_stack(uint32_t cpu_id, uintptr_t stack_top) {
    if (cpu_id >= CPU_ARCH_MAX_CPUS)
        return;

    if (!stack_top)
        stack_top = cpus[cpu_id].default_stack_top;

    cpus[cpu_id].kernel_stack_top = stack_top;
}

void arch_ap_init() {
    arch_init_interrupts();
    timer_init();
}
