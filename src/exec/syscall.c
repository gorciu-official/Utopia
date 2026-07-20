#include <process.h>
#include <scheduler.h>
#include <panic.h>
#include <lib/screen.h>
#include <drivers/ps2.h>
#include <arch/x86_64/registers.h>
#include <memory.h>

//  ---- syscall convention ----
//    rax - return value (signed) & first syscall number (unsigned)
//    then:
//      rdi, rsi, rdx (unsigned)

#define USER_HEAP_MAX 0x0000800000000000ULL

static uint64_t page_align_up(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int grow_process_brk(process_t* process, uint64_t new_break) {
    int map_page_4k(uint64_t* l4_table, uint64_t virt, uint64_t phys, uint64_t flags);

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
    case 12: { // brk
        uint64_t requested_break = regs->rdi;

        if (!current_process) {
            regs->rax_i = -1;
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

    printk("Debug", "RAX return value %d", regs->rax_i);
}
