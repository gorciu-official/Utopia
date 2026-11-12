#include <lib/screen.h>
#include <arch/common.h>
#include <types.h>
#include <kernel/panic.h>
#include <exec/scheduler.h>

#include "plic.h"

#define csrw(csr, val) \
    asm volatile ("csrw " #csr ", %0" : : "r"(val) : "memory")

extern void arch_interrupt_handler_asm(void);
extern void timer_schedule_next(registers_t** regs);
extern void syscall_handler(registers_t* regs);

void arch_init_interrupts() {
    asm volatile("csrw sscratch, zero");
    csrw(0x105, (uintptr_t)arch_interrupt_handler_asm);

    plic_init();
}

// TODO: idk store this elsewhere i'm lazy
#define FAULT_SIGILL  4
#define FAULT_SIGTRAP 5
#define FAULT_SIGBUS  7
#define FAULT_SIGSEGV 11

extern void arch_uart_handler();

static int fault_signal(uint64_t cause) {
    switch (cause) {
    case 2:  return FAULT_SIGILL;  
    case 3:  return FAULT_SIGTRAP;
    case 0: case 4: case 6:
             return FAULT_SIGBUS;       
    default: return FAULT_SIGSEGV;   
    }
}

registers_t* arch_interrupt_handler(registers_t* regs) {
    bool is_interrupt = (regs->scause >> 63) & 1;
    uint64_t cause = regs->scause & ~(1ull << 63);

    if (!is_interrupt) {
        if (cause == 8) {
            syscall_handler(regs);
            regs->sepc += 4;
            return regs;
        }

        if (!(regs->sstatus & (1ULL << 8))) {
            int sig = fault_signal(cause);
            locked_printf(
                "Fatal signal %d at %p (scause=%d, stval=%p)\n",
                sig, regs->sepc, (int)cause, regs->stval
            );
            thread_exit_status(sig);
            return scheduler_schedule(regs);
        }

        panic("Unexpected CPU exception", regs);
    } else {
        switch (cause) {
        case 5:
            timer_schedule_next(&regs);
            break;
        case 9:
            arch_uart_handler();
            break;
        default:
            break;
        }
    }

    return regs;
}
