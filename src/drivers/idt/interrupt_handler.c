#include "registers.h"
#include <types.h>
#include <drivers/screen.h>
#include <drivers/internals/ports.h>

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

void isr_handler(registers_t* regs) {
    printk("ISR interrupt handler", "Interrupt handler invoked for interrupt %d", regs->int_no);

    // -- cpu exceptions
    if (regs->int_no < 32) {
        printk("ISR interrupt handler", "CPU exception: %s", cpu_exception_name(regs->int_no));
        // it is recommended to stop the system if cpu exception occurs in kernel mode 
        // we will do a loop instead
        asm volatile ("cli");
        while (true) continue;
    }  

    // -- hardware interrupts 
    // --- end of interrupt 
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        if (regs->int_no >= 40) {
            outb(0xA0, 0x20);  // slave EOI (we now have modern slavery frfr)
        }
        outb(0x20, 0x20);      // master EOI
    }
}
