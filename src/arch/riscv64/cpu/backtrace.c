#include <types.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <lib/extensions.h>

static const char* cpu_exception_name(uintptr_t int_no) {
    // copied from https://git.evalyngoemer.com/evalynOS/evalynOS/src/branch/main/kernel/src/arch/riscv64/cpu/interrupts.c
    static const char* exceptions[] = {
        "Instruction address misaligned",
        "Instruction access fault",
        "Illegal instruction",
        "Breakpoint",
        "Load address misaligned",
        "Load access fault",
        "Store/AMO address misaligned",
        "Store/AMO access fault",
        "Environment call from U-mode",
        "Environment call from S-mode",
        "Reserved",
        "Reserved",
        "Instruction page fault",
        "Load page fault",
        "Reserved",
        "Store/AMO page fault",
        "Reserved",
        "Reserved",
        "Software check",
        "Hardware error"
    };

    if (int_no < countof(exceptions)) {
        return exceptions[int_no];
    }

    return "Unknown";
}

void arch_print_backtrace(registers_t *regs) {
    printk(KMT_ERROR, "Core", "    - CPU exception: %s", cpu_exception_name(regs->scause));
    printk(KMT_ERROR, "Core", "    - Basic info:    cause=%p  cpu_id=%d  stval=%p", regs->scause, current_processor_id(), regs->stval);
    printk(KMT_ERROR, "Core", "    - Registers:     ra=%p  sp=%p  gp=%p  tp=%p", regs->x[1], regs->x[2], regs->x[3], regs->x[4]);
    printk(KMT_ERROR, "Core", "    - Registers:     a0=%p  a1=%p  a2=%p  a3=%p", regs->x[10], regs->x[11], regs->x[12], regs->x[13]);
    printk(KMT_ERROR, "Core", "    - Registers:     a4=%p  a5=%p  a6=%p  a7=%p", regs->x[14], regs->x[15], regs->x[16], regs->x[17]);
    printk(KMT_ERROR, "Core", "    - Registers:     sepc=%p  stval=%p", regs->sepc, regs->stval);
}
