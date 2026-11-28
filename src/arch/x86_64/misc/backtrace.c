#include <arch/common.h>
#include <arch/x86_64/registers.h>
#include <lib/screen.h>
#include <lib/extensions.h>

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

    if (int_no < countof(exceptions)) {
        return exceptions[int_no];
    }

    return "Unknown";
}

static void print_ommitted(int* ommitted) {
    if (*ommitted == 0) return;

    printk(KMT_ERROR, "Core", "      * Ommited %d NULL entries", *ommitted);
    *ommitted = 0;
}

void arch_print_backtrace(registers_t* regs) {
    printk(KMT_ERROR, "Core", "    - CPU exception: %s", cpu_exception_name(regs->int_no));
    printk(KMT_ERROR, "Core", "    - Basic info:    interrupt_number=%d   apic_cpu_id=%d  err_code=%p", regs->int_no, current_processor_id(), regs->err_code);
    printk(KMT_ERROR, "Core", "    - Registers:     rax=%p  rbx=%p  rcx=%p  rdx=%p", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    printk(KMT_ERROR, "Core", "    - Registers:     rsi=%p  rdi=%p  rbp=%p  rsp=%p", regs->rsi, regs->rdi, regs->rbp, regs->rsp);
    printk(KMT_ERROR, "Core", "    - Registers:     cr2=%p  rip=%p", read_pf_addr(), regs->rip);

    if (regs->int_no == 14) {
        printk(
            KMT_ERROR, "Core", "    - Page fault:    %s mode, %s %p", (regs->err_code & (1 << 2)) ? "user" : "kernel",
            (regs->err_code & (1 << 1)) ? "writing to" : "reading", read_pf_addr()
        );
    }

    printk(KMT_ERROR, "Core", "    - Stack dump (truncated to 16 entries):");

    uint64_t* stack = (uint64_t*)regs->rsp;

    int truncated = 0;

    for (int i = 0; i < 16; i++) {
        uint64_t value = stack[i];

        if (value == 0) {
            truncated++;
            continue;
        }

        print_ommitted(&truncated);

        printk(
            KMT_ERROR, "Core", "      * [%p] = %p",
            stack + i, stack[i]
        );
    }

    print_ommitted(&truncated);
}
