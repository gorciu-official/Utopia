#include <types.h>
#include <arch/x86_64/common.h>
#include <arch/x86_64/registers.h>
#include <lib/screen.h>

static uint32_t invoker = 0;
static bool panicked = false;

static const char* cpu_exception_name(uintptr_t int_no) {
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

    if (int_no < 32) {
        return exceptions[int_no];
    }

    return "Unknown";
}

void panic(const char* reason, registers_t* regs) {
    asm volatile ("cli");
    
    printk_remove_console_suspension();
    printk("Core", "\x1b[31mKernel panic\x1b[0m: %s", reason);
    if (regs != NULL) {
        printk("Core", "  - CPU exception: %s", cpu_exception_name(regs->int_no));
        printk("Core", "  - Basic info: interrupt_number=%d   apic_cpu_id=%d  err_code=%p", regs->int_no, current_processor_id(), regs->err_code);
        printk("Core", "  - Registers:  rax=%p  rbx=%p  rcx=%p  rdx=%p", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        printk("Core", "  - Registers:  rsi=%p  rdi=%p  rbp=%p  rsp=%p", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
        printk("Core", "  - Registers:  cr2=%p  rip=%p", read_cr2(), regs->rip);
    }
    
    invoker = current_processor_id();
    panicked = true; 
    
    printk("Core", "  - The system enters a halted state, please restart the computer manually.");

    while (true)
        asm volatile (
            "cli\n"
            "hlt"
        );

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
