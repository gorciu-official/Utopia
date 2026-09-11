#include <lib/screen.h>
#include <arch/common.h>
#include <types.h>
#include <panic.h>

#define csrw(csr, val) \
    asm volatile ("csrw " #csr ", %0" : : "r"(val) : "memory")

extern void arch_interrupt_handler_asm(void);
extern void timer_schedule_next(registers_t** regs);

void arch_init_interrupts() {
    csrw(0x105, (uintptr_t)arch_interrupt_handler_asm);
}

registers_t* arch_interrupt_handler(registers_t* regs) {
    bool is_interrupt = (regs->scause >> 63) & 1;
    uint64_t cause = regs->scause & ~(1ull << 63);

    if (!is_interrupt) {
        panic("CPU_EXCEPTION", regs);
    } else {
        switch (cause) {
        case 5:
            timer_schedule_next(&regs);
            break;
        default:
            break;
        }
    }

    return regs;
}
