#include <types.h>
#include <exec/scheduler.h>
#include <exec/process.h>
#include <lib/screen.h>
#include <arch/common.h>

#include "common.h"

extern SYSCALL_ABI_DECLARE(own);

#ifdef CONFIG_SYSCALL_ABI_LINUX
extern SYSCALL_ABI_DECLARE(linux);
#endif

static inline syscall_abi_t get_process_abi(process_t* proc) {
    switch (proc->syscall_conv) {
#ifdef CONFIG_SYSCALL_ABI_LINUX
    case 1:
        return syscall_abi_linux;
#endif
    case 0: default:
        return syscall_abi_own;
    }
}

void syscall_handler(registers_t* regs) {
    thread_t* current_thread       = scheduler_get_current_thread();
    process_t* current_process     = current_thread->process;
    syscall_abi_t current_abi      = get_process_abi(current_process);
    
#if ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    uint64_t sstatus;
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
    sstatus |= (1ULL << 18);
    asm volatile("csrw sstatus, %0" :: "r"(sstatus));
    asm volatile("csrr %0, sstatus" : "=r"(sstatus));
#endif

    syscall_regs_t sregs = current_abi.to_sregs(regs);
    sregs.frame = regs;
    uint64_t syscall_num = sregs.syscall_no;

    dprintk(
        "Syscall", "Invoked %d with args arg1=%p arg2=%p arg3=%p", sregs.syscall_no, 
        sregs.arg1, sregs.arg2, sregs.arg3
    );
    dprintk(
        "Syscall", "                     arg4=%p arg5=%p arg6=%p", 
        sregs.arg4, sregs.arg5, sregs.arg6
    );

    uint64_t retval = 0;

    if (
        syscall_num < current_abi.table_size && 
        (*current_abi.table)[syscall_num]
    ) {
        syscall_result_t res = (*current_abi.table)[syscall_num](&sregs, current_process, current_thread);
        retval = res.code;
    } else {
        dprintk("Syscall", "Called non-existing syscall %d", sregs.syscall_no);
        retval = current_abi.not_found_error;
    }

    current_abi.set_ret_value(retval, regs);

    dprintk("Syscall", "Return value %p (decimal %d)", retval, (int64_t)retval);
}
