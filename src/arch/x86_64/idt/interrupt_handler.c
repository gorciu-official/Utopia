#include <drivers/ps2.h>
#include <arch/x86_64/common.h>
#include <arch/x86_64/registers.h>
#include <types.h>
#include <lib/screen.h>
#include <arch/x86_64/io.h>
#include <drivers/timer.h>
#include <scheduler.h>
#include <panic.h>

extern registers_t* lapic_timer_handler(registers_t* regs);

registers_t* isr_handler(registers_t* regs) {
    // -- cpu exceptions
    if (regs->int_no < 32) {
        if (regs->int_no == 14 && regs->err_code & (1 << 2)) {
            printf(
                "Segmentation fault at %p (%s %p; %s)\n", regs->rip,
                (regs->err_code & (1 << 1)) ? "writing to" : "reading", 
                read_pf_addr(), 
                (regs->err_code & (1 << 0)) ? "protection violation" : "non-present"
            );
            thread_exit();
        } else {
            panic("CPU_EXCEPTION", regs);
        }
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
        regs = scheduler_schedule(regs);
    } else if (regs->int_no == 33) {
        ps2_interrupt_handler();
    } else if (regs->int_no == 0x40) {
        regs = lapic_timer_handler(regs);
    }

    return regs;
}
