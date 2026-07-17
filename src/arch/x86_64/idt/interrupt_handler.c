#include <drivers/ps2.h>
#include <arch/x86_64/common.h>
#include <arch/x86_64/registers.h>
#include <types.h>
#include <lib/screen.h>
#include <arch/x86_64/io.h>
#include <drivers/timer.h>
#include <scheduler.h>

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

registers_t* isr_handler(registers_t* regs) {
    // -- cpu exceptions
    if (regs->int_no < 32) {
        printk_remove_console_suspension();
        printk("ISR interrupt handler", "CPU exception: \x1b[31m%s\x1b[0m", cpu_exception_name(regs->int_no));
        printk("ISR interrupt handler", "Basic info: interrupt_number=%d   apic_cpu_id=%d  err_code=%p", regs->int_no, current_processor_id(), regs->err_code);
        printk("ISR interrupt handler", "Registers:  rax=%p  rbx=%p  rcx=%p  rdx=%p", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        printk("ISR interrupt handler", "Registers:  rsi=%p  rdi=%p  rbp=%p  rsp=%p", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
        printk("ISR interrupt handler", "Registers:  cr2=%p  rip=%p", read_cr2(), regs->rip);

        // it is recommended to stop the system if cpu exception occurs in kernel mode 
        // we will do a loop instead

        // TODO: maybe send INIT IPI to APs
        asm volatile ("cli");
        while (true) continue;
    }  

    // --- end of interrupt 
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20);  // slave EOI (we now have modern slavery frfr)
        }
        outb(0x20, 0x20);      // master EOI
    }

    // -- hardware interrupts 
    if (regs->int_no == 32) { 
        timer_handler();
        regs = scheduler_schedule(regs);
    } else if (regs->int_no == 33) {
        ps2_interrupt_handler();
    }

    return regs;
}
