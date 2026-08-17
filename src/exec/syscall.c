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

typedef unsigned long nfds_t;

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

struct pollfd {
    int fd;
    short events;
    short revents;
};

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
}__attribute__((packed));

#define POLLIN    0x001
#define POLLPRI   0x002
#define POLLOUT   0x004
#define POLLERR   0x008
#define POLLHUP   0x010
#define POLLNVAL  0x020

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
        uint64_t phys = kernel_virt_to_phys(pt_pool_alloc());
        if (!phys) return -1;

        memset(phys_to_virt(phys), 0, 0x1000);

        if (map_page_4k(process->page_table, addr, phys, PAGE_RW | PAGE_USER | PAGE_NX) != 0) {
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

SYSCALL_DEFINE(lseek) {
    (void)thread;

    if (!process)
        return -22; // -EINVAL

    int fd = (int)regs->rdi;
    int64_t offset = (int64_t)regs->rsi;
    int whence = (int)regs->rdx;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return -9; // -EBADF

    vnode_t *vnode = process->fds[fd].vnode;
    if (!vnode)
        return -9; // -EBADF

    int64_t new_offset;

    switch (whence) {
    case 0: // SEEK_SET
        new_offset = offset;
        break;

    case 1: // SEEK_CUR
        new_offset = (int64_t)process->fds[fd].offset + offset;
        break;

    case 2: // SEEK_END
        new_offset = (int64_t)vnode->size + offset;
        break;

    default:
        return -22; // -EINVAL
    }

    if (new_offset < 0)
        return -22; // -EINVAL

    process->fds[fd].offset = (uint64_t)new_offset;

    return new_offset;
}

SYSCALL_DEFINE(fstat) {
    (void)thread;
    if (!process) return -22;

    int fd = regs->rdi;
    struct stat* statbuf = (struct stat*)regs->rsi;

    if (!statbuf) return -14; 

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -9; 
    }

    memset(statbuf, 0, sizeof(struct stat));

    if (fd == 0 || fd == 1 || fd == 2) {
        statbuf->st_mode = 020000 | 0666;
        statbuf->st_blksize = 1024;
        return 0;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -9;
    
    statbuf->st_dev  = 1; 
    statbuf->st_ino  = (uint64_t)(uintptr_t)vnode;
    statbuf->st_size = vnode->size;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (vnode->size + 511) / 512;

    if (vnode->type == VNODE_TYPE_DIR) {
        statbuf->st_mode = 0040000 | 0755;
    } else {
        statbuf->st_mode = 0100000 | 0644;
    }

    dprintk("Debug", "fstat on fd %d mode=%p size=%p", fd, statbuf->st_mode, statbuf->st_size);

    return 0;
}

static size_t do_read(process_t* process, int fd, char* buf, size_t count, uintptr_t offset, bool update_offset) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -9; // -EBADF
    }

    if (fd == 0) {
        asm volatile ("sti"); // TODO: temporary fix, syscalls should not enable interrupts
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
    int res = vnode->ops->read(vnode, buf, count, offset, &bytes_read);
    if (res != 0) {
        return res;
    }

    if (update_offset) {
        process->fds[fd].offset += bytes_read;
    }

    return bytes_read;
}

SYSCALL_DEFINE(read) {
    (void)thread;
    int fd = regs->rdi;
    char* buf = (char*)regs->rsi;
    size_t count = regs->rdx;

    return do_read(process, fd, buf, count, process->fds[fd].offset, true);
}

SYSCALL_DEFINE(pread64) {
    (void)thread;
    int fd = regs->rdi;
    char* buf = (char*)regs->rsi;
    size_t count = regs->rdx;
    uintptr_t offset = regs->r10;

    uintptr_t result = do_read(process, fd, buf, count, offset, false);
    return result;
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

SYSCALL_DEFINE(mprotect) {
    (void)thread;
    if (!process) return -22; // -EINVAL
    
    uint64_t addr   = regs->rdi;
    uint64_t length = regs->rsi;
    uint64_t prot   = regs->rdx;
    
    dprintk("Debug", "mprotect called with addr=%p length=%p prot=%p", addr, length, prot);

    if (length == 0) return 0;
    if (addr & 0xFFF) return -22; // -EINVAL

    uint64_t aligned_len = page_align_up(length);

    uint64_t pflags = PAGE_PRESENT | PAGE_USER;
    if (prot & 0x2) pflags |= PAGE_RW;
    if (!(prot & 0x1)) pflags |= PAGE_NX; 

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (protect_page_4k(process->page_table, va, pflags) != 0) {
            return -12; // -ENOMEM
        }
    }

    return 0;
}

SYSCALL_DEFINE(munmap) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    uint64_t addr   = regs->rdi;
    uint64_t length = regs->rsi;
    
    dprintk("Debug", "munmap called with addr=%p length=%p", addr, length);

    if (length == 0) return -22; // -EINVAL
    if (addr & 0xFFF) return -22; // -EINVAL: addr must be page-aligned

    uint64_t aligned_len = page_align_up(length);

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (unmap_page_4k(process->page_table, va) != 0) {
            return -12; // -ENOMEM
        }
    }

    return 0;
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
    dprintk("Debug", "Accessing through open %s, code %d", path, res);
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

    int dfd = (int)regs->rdi;        // Added dfd argument
    const char* path = (const char*)regs->rsi;
    int flags = (int)regs->rdx;
    (void)dfd;                       // Ignore dfd for now if AT_FDCWD is default

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
    dprintk("Debug", "Accessing through openat %s, code %d", path, res);
    if (res != 0) {
        return res;
    }

    process->fds[fd].vnode = vnode;
    process->fds[fd].offset = (flags & O_APPEND) ? vnode->size : 0;
    process->fds[fd].flags = flags;
    process->fds[fd].used = true;

    return fd;
}

SYSCALL_DEFINE(poll) {
    (void)thread;

    struct pollfd *fds = (struct pollfd*)regs->rdi;
    nfds_t nfds = regs->rsi;

    int ready = 0;

    for (size_t i = 0; i < nfds; i++) {
        fds[i].revents = 0;

        int fd = fds[i].fd;
        if (fd < 0) continue;

        if (fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
            fds[i].revents = POLLNVAL;
            ready++;
            continue;
        }

        if (fds[i].events & POLLOUT) {
            fds[i].revents |= POLLOUT;
        }

        if (fds[i].events & POLLIN) {
            if (fd == 0) {
                fds[i].revents |= POLLIN;
            } else {
                vnode_t *vnode = process->fds[fd].vnode;

                if (vnode && vnode->size > process->fds[fd].offset)
                    fds[i].revents |= POLLIN;
            }
        }

        if (fds[i].revents)
            ready++;
    }

    if (ready)
        return ready;

    return 0;
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

    dprintk("Debug", "Calling arch_ptrctl with code %d addr %p", code, addr);

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
    (void)regs; (void)process; (void)thread;
    thread_exit();
    return 0; 
}

SYSCALL_DEFINE(stub_unimplemented) {
    (void)regs; (void)process; (void)thread;
    printk("Syscall", "fixme: syscall %d reached syscall_stub_unimplemented", regs->rax);
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

#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_FIXED      0x10
#define MAP_ANONYMOUS  0x20

SYSCALL_DEFINE(mmap) {
    (void)thread;
    if (!process) return 0;
    uint64_t addr   = regs->rdi;
    uint64_t length = regs->rsi;
    uint64_t prot   = regs->rdx;
    uint64_t flags  = regs->r10;
    int64_t  fd     = (int64_t)regs->r8;
    uint64_t offset = regs->r9;

    if (length == 0) return 0;

    uint64_t target_vaddr;
    if (addr == 0) {
        target_vaddr = process->mmap_current;
        process->mmap_current += page_align_up(length);
    } else {
        target_vaddr = page_align_up(addr);
    }

    uint64_t pflags = PAGE_USER | PAGE_PRESENT;
    if (prot & 0x2) pflags |= PAGE_RW;
    if (!(prot & 0x1)) pflags |= PAGE_NX;

    bool anonymous = (flags & MAP_ANONYMOUS) || fd < 0;
    vnode_t *node = NULL;
    if (!anonymous) {
        node = process->fds[fd].vnode;
        if (!node) return (uint64_t)-1; // -EBADF
    }

    for (uint64_t vaddr = target_vaddr; vaddr < target_vaddr + length; vaddr += 0x1000) {
        uint64_t phys = kernel_virt_to_phys(pt_pool_alloc());
        if (!phys) return 0;
        uint8_t *dst = (uint8_t*)phys_to_virt(phys);
        memset(dst, 0, 0x1000);

        if (node) {
            uint64_t file_off = offset + (vaddr - target_vaddr);
            uint64_t bytes_read = 0;
            if (file_off < node->size) {
                uint64_t want = node->size - file_off;
                if (want > 0x1000) want = 0x1000;
                node->ops->read(node, dst, want, file_off, &bytes_read);
            }
        }

        if (map_page_4k(process->page_table, vaddr, phys, pflags) != 0) {
            return 0;
        }
    }

    return target_vaddr;
}
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

SYSCALL_DEFINE(access) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    const char* pathname = (const char*)regs->rdi;
    int mode = (int)regs->rsi;

    if (!pathname) return -14; // -EFAULT

    dprintk("Debug", "Accessing %s", pathname);

    vnode_t* vnode = NULL;
    int res = vfs_open(pathname, 0, &vnode);
    if (res != 0) {
        return res;
    }

    if (mode == F_OK) {
        return 0;
    }

    uint32_t st_mode;
    if (vnode->type == VNODE_TYPE_DIR) {
        st_mode = 0040000 | 0755;
    } else {
        st_mode = 0100000 | 0644;
    }

    if ((mode & R_OK) && !(st_mode & 0444)) return -13; // -EACCES
    if ((mode & W_OK) && !(st_mode & 0222)) return -13; // -EACCES
    if ((mode & X_OK) && !(st_mode & 0111)) return -13; // -EACCES

    return 0;
}

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100

SYSCALL_DEFINE(newfstatat) {
    (void)thread;
    if (!process) return -22; // -EINVAL

    int dfd = (int)regs->rdi;
    const char* filename = (const char*)regs->rsi;
    struct stat* statbuf = (struct stat*)regs->rdx;
    int flag = (int)regs->r10;

    (void)dfd;  
    (void)flag;

    if (!filename || !statbuf) return -14; // -EFAULT

    vnode_t* vnode = NULL;
    int res = vfs_open(filename, 0, &vnode);
    if (res != 0) {
        return res;
    }

    memset(statbuf, 0, sizeof(struct stat));
    statbuf->st_dev  = 1;  
    statbuf->st_ino  = (uint64_t)(uintptr_t)vnode;
    statbuf->st_size = vnode->size;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (vnode->size + 511) / 512;

    if (vnode->type == VNODE_TYPE_DIR) {
        statbuf->st_mode = 0040000 | 0755;
    } else {
        statbuf->st_mode = 0100000 | 0644;
    }

    return 0;
}

SYSCALL_DEFINE(set_tid_address) {
    (void) process;

    if ((int32_t*)regs->rdi != NULL) {
        *((int32_t*)regs->rdi) = thread->id;
    }

    return thread->id;
}

SYSCALL_DEFINE(getrandom) {
    (void)thread; (void)process;

    uint8_t* buf = (uint8_t*)regs->rdi;
    size_t count = regs->rsi;

    if (!buf) return -14;
    
    for (size_t i = 0; i < count; i++) {
        buf[i] = (char)(i ^ 0x5A); 
    }
    return count; 
}

static const syscall_fn_t syscall_table[] = {
    [0]   = syscall_read,
    [1]   = syscall_write,
    [2]   = syscall_open,
    [3]   = syscall_close,
    [5]   = syscall_fstat,
    [7]   = syscall_poll,
    [8]   = syscall_lseek,
    [9]   = syscall_mmap,
    [10]  = syscall_mprotect, 
    [11]  = syscall_munmap,
    [12]  = syscall_brk,
    [13]  = syscall_stub_unimplemented,
    [16]  = syscall_stub_unimplemented,
    [17]  = syscall_pread64,
    [20]  = syscall_writev,
    [21]  = syscall_access,
    [60]  = syscall_exit,
    [63]  = syscall_uname,
    [102] = syscall_stub_unimplemented, // that is indeed correct. let me explain.
                                        // this returns 0 and 0 means root
    [158] = syscall_arch_prctl,
    [202] = syscall_stub_unimplemented,
    [218] = syscall_set_tid_address,
    [231] = syscall_exit,
    [257] = syscall_openat,
    [262] = syscall_newfstatat,
    [273] = syscall_stub_unimplemented,
    [318] = syscall_getrandom
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_table[0]))

void syscall_handler(registers_t* regs) {
    thread_t* current_thread = scheduler_get_current_thread();
    process_t* current_process = current_thread->process;

    uint64_t syscall_num = regs->rax;

    dprintk("Debug", "thread=%p process=%p", current_thread, current_process);
    dprintk("Debug", "Syscall %d invoked, RIP=%p RDI=%p RSI=%p RDX=%p", regs->rax, regs->rip, regs->rdi, regs->rsi, regs->rdx);

    if (syscall_num < SYSCALL_TABLE_SIZE && syscall_table[syscall_num]) {
        int64_t res = syscall_table[syscall_num](regs, current_process, current_thread);
        regs->rax_i = res;
    } else {
        printk("Syscall", "fixme: syscall %d reached -ENOSYS (function not implemented)", regs->rax);
        regs->rax_i = ENOSYS_ERR;
    }

    dprintk("Debug", "RAX return value %p (decimal %d)", regs->rax, regs->rax_i);
}
