#include <process.h>
#include <scheduler.h>
#include <panic.h>
#include <lib/screen.h>
#include <drivers/ps2.h>
#include <arch/x86_64/registers.h>

//  ---- syscall convention ----
//    rax - return value (signed) & first syscall number (unsigned)
//    then:
//      rdi, rsi, rdx (unsigned)

void syscall_handler(registers_t* regs) {
    thread_t* current_thread = scheduler_get_current_thread();
    process_t* current_process = current_thread->process;

    printk("Debug", "Syscall %d invoked", regs->rax);

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
    case 60: case 231: { // exit
        if (current_process && current_process->pid == 1) {
            panic("INIT_EXITED", NULL);
        }

        thread_exit();
        break;
    }
    default:
        regs->rax_i = -1;
        break;
    }
}
