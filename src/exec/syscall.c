#include <scheduler.h>
#include <drivers/screen.h>
#include <registers.h>

void syscall_handler(registers_t* regs) {
    switch (regs->rax) {
    case 1:
        switch (regs->rdi) {
        case 1: case 2:
            user_print((char*)regs->rsi, regs->rdx);
            regs->rax_i = regs->rdx;
            break;
        default:
            regs->rax_i = -1;
            break;
        }
        break;
    case 60:
        thread_exit();
        break;
    default:
        regs->rax_i = -1;
        break;
    }
}
