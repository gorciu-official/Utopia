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

uintptr_t write(void* data, uintptr_t len, uintptr_t fd) {
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

void syscall_handler(registers_t* regs) {
    thread_t* current_thread = scheduler_get_current_thread();
    process_t* current_process = current_thread->process;

    dprintk("Debug", "Syscall %d invoked", regs->rax);

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
            regs->rax_i = -38;
            break;
        }
        break;
    case 1: // write
        regs->rax_i = write((void*)regs->rsi, regs->rdx, regs->rdi);
        break;
    case 10: case 202: {
        // TODO: implement mprotect and futex syscall
        break;
    }
    case 20: { // writev
        int fd = regs->rdi;
        iovec_t* iov = (iovec_t*)regs->rsi;
        int iovcnt = regs->rdx;
    
        size_t total = 0;
    
        for (int i = 0; i < iovcnt; i++) {
            size_t ret = write(
                iov[i].iov_base,
                iov[i].iov_len,
                fd
            );
    
            total += ret;
    
            if ((size_t)ret < iov[i].iov_len) {
                break;
            }
        }
    
        regs->rax_i = total;
        break;
    }
    case 12: { // brk
        uint64_t requested_break = regs->rdi;

        if (!current_process) {
            regs->rax_i = -38;
            break;
        }

        if (requested_break == 0) {
            regs->rax = current_process->brk_current;
            break;
        }

        if (requested_break < current_process->brk_start || requested_break >= USER_HEAP_MAX) {
            regs->rax = current_process->brk_current;
            break;
        }

        if (requested_break > current_process->brk_current && grow_process_brk(current_process, requested_break) != 0) {
            regs->rax = current_process->brk_current;
            break;
        }

        current_process->brk_current = requested_break;
        regs->rax = current_process->brk_current;
        break;
    }
    case 158: { // arch_prctl
        uint64_t code = regs->rdi;
        uint64_t addr = regs->rsi;
    
        switch (code) {
        case ARCH_SET_FS:
            write_msr(IA32_FS_BASE, addr);
            regs->rax_i = 0;
            break;
    
        case ARCH_GET_FS: {
            uint64_t fs = read_msr(IA32_FS_BASE);
            if (!addr) {
                regs->rax_i = -1;
                break;
            }
    
            *(uint64_t*)addr = fs;
            regs->rax_i = 0;
            break;
        }
    
        case ARCH_SET_GS:
            write_msr(IA32_GS_BASE, addr);
            regs->rax_i = 0;
            break;
    
        case ARCH_GET_GS: {
            uint64_t gs = read_msr(IA32_GS_BASE);
            if (!addr) {
                regs->rax_i = -1;
                break;
            }
    
            *(uint64_t*)addr = gs;
            regs->rax_i = 0;
            break;
        }
    
        default:
            regs->rax_i = -38;
            break;
        }
    
        break;
    }
    case 60: case 231: { // exit
        if (current_process && current_process->pid == 1) {
            panic("INIT_EXITED", NULL);
        }

        thread_exit();
        break;
    }
    default:
        regs->rax_i = -38;
        break;
    }

    dprintk("Debug", "RAX return value %d", regs->rax_i);
}
