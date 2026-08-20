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

    dprintk(
        "Syscall", "Invoked %d with args arg1=%p arg2=%p arg3=%p", sregs.syscall_no, 
        sregs.arg1, sregs.arg2, sregs.arg3
    );
    dprintk(
        "Syscall", "                     arg4=%p arg5=%p arg6=%p", 
        sregs.arg4, sregs.arg5, sregs.arg6
    );

    if (
        syscall_num < current_abi.table_size && 
        (*current_abi.table)[syscall_num]
    ) {
        int64_t res = (*current_abi.table)[syscall_num](&sregs, current_process, current_thread);
        current_abi.set_ret_value(res, regs); 
    } else {
        dprintk("Syscall", "Called non-existing syscall %d", regs->rax);
        current_abi.set_ret_value(current_abi.not_found_error, regs); 
    }

    dprintk("Syscall", "Return value %p (decimal %d)", regs->rax, regs->rax_i);
}
