#include <types.h>
#include <scheduler.h>
#include <process.h>
#include <lib/screen.h>
#include <arch/x86_64/registers.h>

#include "common.h"

extern SYSCALL_ABI_DECLARE(linux);

static inline syscall_abi_t get_process_abi(process_t* proc) {
    (void)proc;

    // TODO: do not hardcode it, read from process field or smth
    return syscall_abi_linux;
}

void syscall_handler(registers_t* regs) {
    thread_t* current_thread       = scheduler_get_current_thread();
    process_t* current_process     = current_thread->process;
    syscall_abi_t current_abi      = get_process_abi(current_process);

    syscall_regs_t sregs = current_abi.to_sregs(regs);
    uint64_t syscall_num = sregs.syscall_no;

    dprintk("Syscall", "thread=%p process=%p", current_thread, current_process);
    dprintk("Syscall", "Syscall %d invoked, RIP=%p RDI=%p RSI=%p RDX=%p", regs->rax, regs->rip, regs->arg1, regs->arg2, regs->arg3);

    // TODO: for now RAX is hardcoded, this will be changed later or smth 
    if (
        syscall_num < current_abi.table_size && 
        (*current_abi.table)[syscall_num]
    ) {
        int64_t res = (*current_abi.table)[syscall_num](&sregs, current_process, current_thread);
        regs->rax_i = res;
    } else {
        dprintk("Syscall", "Called non-existing syscall %d", regs->rax);
        regs->rax_i = current_abi.not_found_error;
    }

    dprintk("Syscall", "Return value %p (decimal %d)", regs->rax, regs->rax_i);
}
