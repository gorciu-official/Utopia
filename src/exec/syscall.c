#include <process.h>
#include <scheduler.h>
#include <panic.h>
#include <lib/screen.h>
#include <drivers/ps2.h>
#include <arch/x86_64/registers.h>
#include <arch/x86_64/msr.h>
#include <memory.h>

//  ---- syscall convention ----
//    rax - return value (signed) & first syscall number (unsigned)
//    then:
//      rdi, rsi, rdx (unsigned)

typedef struct {
    void* iov_base;
    size_t iov_len;
} iovec_t;

#define USER_HEAP_MAX 0x0000800000000000ULL
#define ENOSYS_ERR    (-38)

typedef uintptr_t (*syscall_fn_t)(registers_t* regs, process_t* process, thread_t* thread);

#define SYSCALL_DEFINE(syscall_name) \
    static uintptr_t syscall_##syscall_name(registers_t* regs, process_t* process, thread_t* thread) 

static uint64_t page_align_up(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int grow_process_brk(process_t* process, uint64_t new_break) {
    uint64_t old_end = page_align_up(process->brk_current);
    uint64_t new_end = page_align_up(new_break);

    for (uint64_t addr = old_end; addr < new_end; addr += 0x1000) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -1;

        memset(phys_to_virt(phys), 0, 0x1000);

        if (map_page_4k(process->page_table, addr, phys, PAGE_RW | PAGE_USER | PAGE_NX) != 0) {
            pmm_free_page(phys);
            return -1;
        }
    }

    return 0;
}

static uintptr_t write_internal(void* data, uintptr_t len, uintptr_t fd) {
    uint64_t result = -1;

    switch (fd) {
    case 1: case 2:
        user_print(data, len);
        result = len;
        break;
    default:
        break;
    }

    return result;
}

SYSCALL_DEFINE(read) {
    (void)process; (void)thread;

    switch (regs->rdi) {
    case 0: {
        asm volatile ("sti"); // TODO: temporary fix, syscalls
                              // should not enable interrupts
        uintptr_t ret = ps2_read((char*)regs->rsi, regs->rdx);
        asm volatile ("cli");
        return ret;
    }
    default:
        return ENOSYS_ERR;
    }
}

SYSCALL_DEFINE(write) {
    (void)process; (void)thread;
    return write_internal((void*)regs->rsi, regs->rdx, regs->rdi);
}

SYSCALL_DEFINE(writev) {
    (void)process; (void)thread;

    int fd = regs->rdi;
    iovec_t* iov = (iovec_t*)regs->rsi;
    int iovcnt = regs->rdx;

    size_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        size_t ret = write_internal(iov[i].iov_base, iov[i].iov_len, fd);
        total += ret;

        if ((size_t)ret < iov[i].iov_len) {
            break;
        }
    }

    return total;
};

SYSCALL_DEFINE(brk) {
    (void)thread;

    uint64_t requested_break = regs->rdi;

    if (!process) {
        return ENOSYS_ERR;
    }

    if (requested_break == 0) {
        return process->brk_current;
    }

    if (requested_break < process->brk_start || requested_break >= USER_HEAP_MAX) {
        return process->brk_current;
    }

    if (requested_break > process->brk_current && grow_process_brk(process, requested_break) != 0) {
        return process->brk_current;
    }

    process->brk_current = requested_break;
    return process->brk_current;
}

SYSCALL_DEFINE(arch_prctl) {
    (void)process; (void)thread;

    uint64_t code = regs->rdi;
    uint64_t addr = regs->rsi;

    switch (code) {
    case ARCH_SET_FS:
        write_msr(IA32_FS_BASE, addr);
        return 0;

    case ARCH_GET_FS: {
        if (!addr) return -1;
        *(uint64_t*)addr = read_msr(IA32_FS_BASE);
        return 0;
    }

    case ARCH_SET_GS:
        write_msr(IA32_GS_BASE, addr);
        return 0;

    case ARCH_GET_GS: {
        if (!addr) return -1;
        *(uint64_t*)addr = read_msr(IA32_GS_BASE);
        return 0;
    }

    default:
        return ENOSYS_ERR;
    }
}

SYSCALL_DEFINE(exit) {
    (void)regs; (void)thread;

    if (process && process->pid == 1) {
        panic("INIT_EXITED", NULL);
    }

    thread_exit();
    return 0; 
}

SYSCALL_DEFINE(stub_unimplemented) {
    (void)regs; (void)process; (void)thread;
    // TODO: implement mprotect and futex syscall
    return 0;
}

static const syscall_fn_t syscall_table[] = {
    [0]   = syscall_read,
    [1]   = syscall_write,
    [10]  = syscall_stub_unimplemented, 
    [12]  = syscall_brk,
    [20]  = syscall_writev,
    [60]  = syscall_exit,
    [158] = syscall_arch_prctl,
    [202] = syscall_stub_unimplemented, 
    [231] = syscall_exit,
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_handler(registers_t* regs) {
    thread_t* current_thread = scheduler_get_current_thread();
    process_t* current_process = current_thread->process;

    uint64_t syscall_num = regs->rax;

    dprintk("Debug", "Syscall %d invoked", regs->rax);

    if (syscall_num < SYSCALL_TABLE_SIZE && syscall_table[syscall_num]) {
        regs->rax_i = syscall_table[syscall_num](regs, current_process, current_thread);
    } else {
        regs->rax_i = ENOSYS_ERR;
    }

    dprintk("Debug", "RAX return value %d", regs->rax_i);
}
