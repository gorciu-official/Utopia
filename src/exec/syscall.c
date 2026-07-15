#include <scheduler.h>
#include <drivers/screen.h>
#include <drivers/ps2.h>
#include <registers.h>

//  ---- syscall convention ----
//    rax - return value (signed) & first syscall number (unsigned)
//    then:
//      rdi, rsi, rdx (unsigned)

void syscall_handler(registers_t* regs) {
    switch (regs->rax) {
    case 0: // read
        switch (regs->rdi) {
        case 0:
            asm volatile ("sti"); // TODO: temporary fix, syscalls 
                                  // should not enable interrupts
            regs->rax_i = ps2_read((char*)regs->rsi, regs->rdx);
            asm volatile ("cli");
            break;
        default:
            regs->rax_i = -1;
            break;
        }
        break;
    case 1: // write
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
    case 60: // exit
        thread_exit();
        break;
    default:
        regs->rax_i = -1;
        break;
    }
}
