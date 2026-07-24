#include <constants.h>
#include <process.h>
#include <scheduler.h>
#include <panic.h>
#include <lib/screen.h>
#include <drivers/ps2.h>
#include <drivers/filesystem.h>
#include <arch/x86_64/registers.h>
#include <arch/x86_64/msr.h>
#include <memory.h>
#include <string.h>

//  ---- syscall convention ----
//    rax - return value (signed) & first syscall number (unsigned)
//    then:
//      rdi, rsi, rdx (unsigned)

typedef struct {
    void* iov_base;
    size_t iov_len;
} iovec_t;

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

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

static uintptr_t write_internal(process_t* process, void* data, uintptr_t len, int fd) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -9; // -EBADF
    }

    if (fd == 1 || fd == 2) {
        user_print(data, len);
        return len;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -9; // -EBADF

    if (vnode->type == VNODE_TYPE_DIR) {
        return -21; // -EISDIR
    }

    if (!vnode->ops || !vnode->ops->write) {
        return -30; // -EROFS
    }

    uint64_t bytes_written = 0;
    int res = vnode->ops->write(vnode, data, len, process->fds[fd].offset, &bytes_written);
    if (res != 0) {
        return res;
    }

    process->fds[fd].offset += bytes_written;
    return bytes_written;
}

SYSCALL_DEFINE(read) {
    (void)thread;
    if (!process) return -1;

    int fd = regs->rdi;
    char* buf = (char*)regs->rsi;
    size_t count = regs->rdx;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -9; // -EBADF
    }

    if (fd == 0) {
        asm volatile ("sti"); // TODO: temporary fix, syscalls
                              // should not enable interrupts
        uintptr_t ret = ps2_read(buf, count);
        asm volatile ("cli");
        return ret;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -9; // -EBADF

    if (vnode->type == VNODE_TYPE_DIR) {
        return -21; // -EISDIR
    }

    if (!vnode->ops || !vnode->ops->read) {
        return -22; // -EINVAL
    }

    uint64_t bytes_read = 0;
    int res = vnode->ops->read(vnode, buf, count, process->fds[fd].offset, &bytes_read);
    if (res != 0) {
        return res;
    }

    process->fds[fd].offset += bytes_read;
    return bytes_read;
}

SYSCALL_DEFINE(write) {
    (void)thread;
    return write_internal(process, (void*)regs->rsi, regs->rdx, regs->rdi);
}

SYSCALL_DEFINE(writev) {
    (void)thread;
    if (!process) return -1;

    int fd = regs->rdi;
    iovec_t* iov = (iovec_t*)regs->rsi;
    int iovcnt = regs->rdx;

    size_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        size_t ret = write_internal(process, iov[i].iov_base, iov[i].iov_len, fd);
        if (ret == (uintptr_t)-9) {
            if (total == 0) return -9;
            break;
        }
        total += ret;

        if ((size_t)ret < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

SYSCALL_DEFINE(open) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    const char* path = (const char*)regs->rdi;
    int flags = regs->rsi;

    if (!path) return -14; // -EFAULT

    int fd = -1;
    for (int i = 3; i < MAX_FILES_PER_PROCESS; i++) {
        if (!process->fds[i].used) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -24; // -EMFILE
    }

    vnode_t* vnode = NULL;
    int res = vfs_open(path, flags, &vnode);
    if (res != 0) {
        return res;
    }

    process->fds[fd].vnode = vnode;
    process->fds[fd].offset = (flags & O_APPEND) ? vnode->size : 0;
    process->fds[fd].flags = flags;
    process->fds[fd].used = true;

    return fd;
}

SYSCALL_DEFINE(close) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    int fd = regs->rdi;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -9; // -EBADF
    }

    process->fds[fd].vnode = NULL;
    process->fds[fd].offset = 0;
    process->fds[fd].flags = 0;
    process->fds[fd].used = false;

    return 0;
}

SYSCALL_DEFINE(openat) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    const char* path = (const char*)regs->rsi;
    int flags = regs->rdx;

    if (!path) return -14; // -EFAULT

    int fd = -1;
    for (int i = 3; i < MAX_FILES_PER_PROCESS; i++) {
        if (!process->fds[i].used) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -24; // -EMFILE
    }

    vnode_t* vnode = NULL;
    int res = vfs_open(path, flags, &vnode);
    if (res != 0) {
        return res;
    }

    process->fds[fd].vnode = vnode;
    process->fds[fd].offset = (flags & O_APPEND) ? vnode->size : 0;
    process->fds[fd].flags = flags;
    process->fds[fd].used = true;

    return fd;
}

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

SYSCALL_DEFINE(uname) {
    (void)process; (void)thread;

    struct utsname* ptr = (struct utsname*)regs->rdi;

    strcpy(ptr->sysname, "Utopia");
    strcpy(ptr->nodename, "example-computer-lol");
    strcpy(ptr->release, UTOPIA_VERSION);
    strcpy(ptr->version, "(no build information)");
    strcpy(ptr->machine, "x86_64");
    strcpy(ptr->domainname, "");

    return 0;
}

SYSCALL_DEFINE(mmap) {
    (void)thread;

    if (!process) return 0;

    uint64_t addr = regs->rdi;
    uint64_t length = regs->rsi;
    uint64_t prot = regs->rdx;

    if (length == 0) return 0;

    uint64_t target_vaddr;
    if (addr == 0) {
        target_vaddr = process->mmap_current;
        process->mmap_current += page_align_up(length);
    } else {
        target_vaddr = page_align_up(addr);
    }

    uint64_t flags = PAGE_USER | PAGE_PRESENT;
    if (prot & 0x2) flags |= PAGE_RW; // PF_W/PROT_WRITE
    if (!(prot & 0x1)) flags |= PAGE_NX; // PF_X/PROT_EXEC

    for (uint64_t vaddr = target_vaddr; vaddr < target_vaddr + length; vaddr += 0x1000) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return 0;

        memset(phys_to_virt(phys), 0, 0x1000);

        if (map_page_4k(process->page_table, vaddr, phys, flags) != 0) {
            pmm_free_page(phys);
            return 0;
        }
    }

    return target_vaddr;
}

static const syscall_fn_t syscall_table[] = {
    [0]   = syscall_read,
    [1]   = syscall_write,
    [2]   = syscall_open,
    [3]   = syscall_close,
    [7]   = syscall_stub_unimplemented,
    [9]   = syscall_mmap,
    [10]  = syscall_stub_unimplemented, 
    [11]  = syscall_stub_unimplemented,
    [12]  = syscall_brk,
    [13]  = syscall_stub_unimplemented, 
    [20]  = syscall_writev,
    [60]  = syscall_exit,
    [63]  = syscall_uname,
    [102] = syscall_stub_unimplemented, // that is indeed correct. let me explain.
                                        // this returns 0 and 0 means root
    [158] = syscall_arch_prctl,
    [202] = syscall_stub_unimplemented,
    [231] = syscall_exit,
    [257] = syscall_openat,
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
