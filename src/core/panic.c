#include <types.h>
#include <lib/screen.h>
#include <constants.h>
#include <arch/common.h>

static uint32_t invoker = 0;
static bool panicked = false;

static const char* cpu_exception_name(uintptr_t int_no) {
#if ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
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
#elif ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    static const char* exceptions[] = {
        "Divide Error",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun (reserved)",
        "Invalid TSS",
        "Segment Not Present",
        "Stack-Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 Floating-Point Exception",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception",
        "Virtualization Exception",
        "Control Protection Exception",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Hypervisor Injection Exception",
        "VMM Communication Exception",
        "Security Exception",
        "Reserved"
    };
#endif

    if (int_no < 32) {
        return exceptions[int_no];
    }

    return "Unknown";
}

void panic(const char* reason, registers_t* regs) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    asm volatile ("cli");
#endif
    
    printk_remove_console_suspension();
    printk("Core", "\x1b[91mKernel panic\x1b[0m: %s", reason);
    if (regs != NULL) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        printk("Core", "  - CPU exception: %s", cpu_exception_name(regs->int_no));
        printk("Core", "  - Basic info:    interrupt_number=%d   apic_cpu_id=%d  err_code=%p", regs->int_no, current_processor_id(), regs->err_code);
        printk("Core", "  - Registers:     rax=%p  rbx=%p  rcx=%p  rdx=%p", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        printk("Core", "  - Registers:     rsi=%p  rdi=%p  rbp=%p  rsp=%p", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
        printk("Core", "  - Registers:     cr2=%p  rip=%p", read_pf_addr(), regs->rip);

        if (regs->int_no == 14) {
            printk(
                "Core", "  - Page fault:    %s mode, %s %p", (regs->err_code & (1 << 2)) ? "user" : "kernel",
                (regs->err_code & (1 << 1)) ? "writing to" : "reading", read_pf_addr()
            );
        }
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
        printk("Core", "  - CPU exception: %s", cpu_exception_name(regs->scause));
        printk("Core", "  - Basic info:    cause=%p  cpu_id=%d  stval=%p", regs->scause, current_processor_id(), regs->stval);
        printk("Core", "  - Registers:     ra=%p  sp=%p  gp=%p  tp=%p", regs->x[1], regs->x[2], regs->x[3], regs->x[4]);
        printk("Core", "  - Registers:     a0=%p  a1=%p  a2=%p  a3=%p", regs->x[10], regs->x[11], regs->x[12], regs->x[13]);
        printk("Core", "  - Registers:     a4=%p  a5=%p  a6=%p  a7=%p", regs->x[14], regs->x[15], regs->x[16], regs->x[17]);
        printk("Core", "  - Registers:     sepc=%p  stval=%p", regs->sepc, regs->stval);
#endif
    } else {
        printk("Core", "  - No register dump available, panic not invoked via hardware interrupt.");
    }
    
    invoker = current_processor_id();
    panicked = true; 
   
    printk("Core", "  - You can try to file a bug report: https://github.com/gorciu-official/Utopia/issues/new");
    printk("Core", "  - The system enters a halted state, please restart the computer manually.");

    while (true)
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        asm volatile (
            "cli\n"
            "hlt"
        );
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
        // TODO: there should be a disable interrupts thingy
        asm volatile ("ebreak");
#endif

    // TODO: maybe send init IPIs to APs?
    //
    //       i had an idea of using check_panic when new PIT interrupts 
    //       arrive and shutting down APs this way, but PIT interrupts
    //       apparently work only on BSP
    //       (i am too lazy to implement LAPIC clockevent)
    //
    //       though, for the time I'm writing this, you can't do multi-thread
    //       userspace yet, so theoretically there is no need to shutdown APs 
    //       now

    __builtin_unreachable();
}
