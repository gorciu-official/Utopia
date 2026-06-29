#include <scheduler.h>
#include <drivers/screen.h>
#include <registers.h>

void syscall_handler(registers_t* regs) {
    printk("Syscall handler", "Syscall instruction was called (rax: 0x%x)", regs->rax);
    switch (regs->rax) {
        case 1:
            printk("Ring 3 program message", "%s", (char*)regs->rsi);
            break;
        case 60:
            thread_exit();
            break;
        default: 
            break;
    }
}

