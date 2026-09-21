#include <scheduler.h>
#include <constants.h>
#include <arch/common.h>
#include <lib/screen.h>

#include "common.h"

SYSCALL_DEFINE_OWN(exit) {
    (void)regs; (void)process; (void)thread;
    thread_exit();
    return 0;
}

SYSCALL_DEFINE_OWN(write) {
    (void)process; (void)thread;

    uintptr_t fd = regs->arg1;
    char* data = (char*)regs->arg2; 
    uintptr_t len = regs->arg3;

    if (fd == 1 || fd == 2) {
        user_print(data, len);
        return len;
    }

    return -1;
}

static const syscall_fn_t syscall_own_table[] = {
    [5]    = syscall_own_write,
    [2000] = syscall_own_exit
};

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
static syscall_regs_t syscall_own_to_sregs(registers_t* regs) {
    return (syscall_regs_t) {
        .syscall_no = regs->rax,
        .arg1 = regs->rdx, .arg2 = regs->rdi, .arg3 = regs->rsi,
        .arg4 = regs->r10, .arg5 = regs->r9,  .arg6 = regs->r8
    };
}

static void syscall_own_set_return_val(int64_t val, registers_t* regs) {
    regs->rax = (uint64_t)val;
}
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
static syscall_regs_t syscall_linux_to_sregs(registers_t* regs) {
    return (syscall_regs_t) {
        .syscall_no = regs->x[10], // a0

        .arg1 = regs->x[11], /* a1 */ .arg2 = regs->x[12], /* a2 */ .arg3 = regs->x[13], /* a3 */ 
        .arg4 = regs->x[14], /* a4 */ .arg5 = regs->x[15], /* a5 */ .arg6 = regs->x[17], /* a7 */
    };
}

static void syscall_own_set_return_val(int64_t val, registers_t* regs) {
    regs->x[10] = (uint64_t)val;
}
#endif

SYSCALL_ABI_DEFINE(own, syscall_own_table, syscall_own_to_sregs, syscall_own_set_return_val, -1)
