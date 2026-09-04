#include <arch/common.h>
#include <types.h>
#include <panic.h>

#define csrw(csr, val) \
    asm volatile ("csrw " #csr ", %0" : : "r"(val) : "memory")

extern void arch_interrupt_handler_asm(void);

void arch_init_interrupts() {
    csrw(0x105, (uintptr_t)arch_interrupt_handler_asm);
}

void arch_interrupt_handler(registers_t* regs) {
    bool is_interrupt = (regs->scause >> 63) & 1;
    // for future use: uint64_t cause = regs->scause & ~(1ull << 63);

    // either cpu exception or timer, timer is unimplemented
    // so nearly 100% cpu exception
    if (!is_interrupt) {
        panic("CPU_EXCEPTION", regs);
    }
}
