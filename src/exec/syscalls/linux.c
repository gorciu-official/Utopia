#include "arch/memory.h"
#include <constants.h>
#include <process.h>
#include <scheduler.h>
#include <panic.h>
#include <lib/screen.h>
#include <drivers/ps2.h>
#include <drivers/filesystem.h>
#include <memory.h>
#include <lib/string.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#include <arch/x86_64/common.h>
#include <arch/x86_64/msr.h>
#endif

#include "linux.h"
#include "common.h"

static uint64_t page_align_up(uint64_t value) {
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int grow_process_brk(process_t* process, uint64_t new_break) {
    uint64_t old_end = page_align_up(process->brk_current);
    uint64_t new_end = page_align_up(new_break);

    for (uint64_t addr = old_end; addr < new_end; addr += 0x1000) {
        uint64_t phys = hhdm_virt_to_phys(pt_pool_alloc());
        if (!phys) return -1;

        memset(phys_to_virt(phys), 0, 0x1000);

        if (map_page_4k(process->page_table, addr, phys, VMF_USER | VMF_WRITE) != 0) {
            return -1;
        }
    }

    return 0;
}

static uintptr_t write_internal(process_t* process, void* data, uintptr_t len, int fd) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; // -EBADF
    }

    if (fd == 1 || fd == 2) {
        user_print(data, len);
        return len;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -EBADF; // -EBADF

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

SYSCALL_DEFINE_LINUX(lseek) {
    (void)thread;

    if (!process)
        return -EINVAL;

    int fd = (int)regs->arg1;
    int64_t offset = (int64_t)regs->arg2;
    int whence = (int)regs->arg3;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return -EBADF;

    vnode_t *vnode = process->fds[fd].vnode;
    if (!vnode)
        return -EBADF;

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
        return -EINVAL;
    }

    if (new_offset < 0)
        return -EINVAL;

    process->fds[fd].offset = (uint64_t)new_offset;

    return new_offset;
}

SYSCALL_DEFINE_LINUX(getcwd) {
    (void)thread; (void)process;

    char *buf = (char *)regs->arg1;
    size_t size = regs->arg2;

    if (size < 2)
        return -34;

    buf[0] = '/';
    buf[1] = '\0';

    return 2;
}

SYSCALL_DEFINE_LINUX(fstat) {
    (void)thread;
    if (!process) return -EINVAL;

    int fd = regs->arg1;
    struct stat* statbuf = (struct stat*)regs->arg2;

    if (!statbuf) return -EFAULT; 

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    memset(statbuf, 0, sizeof(struct stat));

    if (fd == 0 || fd == 1 || fd == 2) {
        statbuf->st_mode = 020000 | 0666;
        statbuf->st_blksize = 1024;
        return 0;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -EBADF;
    
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

    dprintk("Syscall", "fstat on fd %d mode=%p size=%p", fd, statbuf->st_mode, statbuf->st_size);

    return 0;
}

static size_t do_read(process_t* process, int fd, char* buf, size_t count, uintptr_t offset, bool update_offset) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    if (fd == 0) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        asm volatile ("sti"); // TODO: temporary fix, syscalls should not enable interrupts
        uintptr_t ret = ps2_read(buf, count);
        asm volatile ("cli");
        return ret;
#endif
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -EBADF; 

    if (vnode->type == VNODE_TYPE_DIR) {
        return -21; 
    }

    if (!vnode->ops || !vnode->ops->read) {
        return -EINVAL; 
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

SYSCALL_DEFINE_LINUX(read) {
    (void)thread;
    int fd = regs->arg1;
    char* buf = (char*)regs->arg2;
    size_t count = regs->arg3;

    return do_read(process, fd, buf, count, process->fds[fd].offset, true);
}

SYSCALL_DEFINE_LINUX(pread64) {
    (void)thread;
    int fd = regs->arg1;
    char* buf = (char*)regs->arg2;
    size_t count = regs->arg3;
    uintptr_t offset = regs->arg4;

    uintptr_t result = do_read(process, fd, buf, count, offset, false);
    return result;
}

SYSCALL_DEFINE_LINUX(write) {
    (void)thread;
    return write_internal(process, (void*)regs->arg2, regs->arg3, regs->arg1);
}

SYSCALL_DEFINE_LINUX(writev) {
    (void)thread;
    if (!process) return -1;

    int fd = regs->arg1;
    iovec_t* iov = (iovec_t*)regs->arg2;
    int iovcnt = regs->arg3;

    size_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        size_t ret = write_internal(process, iov[i].iov_base, iov[i].iov_len, fd);
        if (ret == (uintptr_t)-EBADF) {
            if (total == 0) return -EBADF;
            break;
        }
        total += ret;

        if ((size_t)ret < iov[i].iov_len) {
            break;
        }
    }

    return total;
}

SYSCALL_DEFINE_LINUX(mprotect) {
    (void)thread;
    if (!process) return -EINVAL;
    
    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;
    
    dprintk("Syscall", "mprotect called with addr=%p length=%p prot=%p", addr, length, prot);

    if (length == 0) return 0;
    if (addr & 0xFFF) return -EINVAL; 

    uint64_t aligned_len = page_align_up(length);

    uint64_t pflags = VMF_USER;
    if (prot & 0x2) pflags |= VMF_WRITE;
    if (prot & 0x1) pflags |= VMF_EXEC; 

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (protect_page_4k(process->page_table, va, pflags) != 0) {
            return -ENOMEM;
        }
    }

    return 0;
}

SYSCALL_DEFINE_LINUX(munmap) {
    (void)thread;
    if (!process) return -EINVAL; 

    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    
    dprintk("Syscall", "munmap called with addr=%p length=%p", addr, length);

    if (length == 0) return -EINVAL;
    if (addr & 0xFFF) return -EINVAL; 

    uint64_t aligned_len = page_align_up(length);

    for (uint64_t va = addr; va < addr + aligned_len; va += 0x1000) {
        if (unmap_page_4k(process->page_table, va) != 0) {
            return -12;
        }
    }

    return 0;
}

SYSCALL_DEFINE_LINUX(open) {
    (void)thread;
    if (!process) return -EINVAL;

    const char* path = (const char*)regs->arg1;
    int flags = regs->arg2;

    if (!path) return -EFAULT; 

    int fd = -1;
    for (int i = 3; i < MAX_FILES_PER_PROCESS; i++) {
        if (!process->fds[i].used) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -EMFILE; 
    }

    vnode_t* vnode = NULL;
    int res = vfs_open(path, flags, &vnode);
    dprintk("Syscall", "Accessing through open %s, code %d", path, res);
    if (res != 0) {
        return res;
    }

    process->fds[fd].vnode = vnode;
    process->fds[fd].offset = (flags & O_APPEND) ? vnode->size : 0;
    process->fds[fd].flags = flags;
    process->fds[fd].used = true;

    return fd;
}

SYSCALL_DEFINE_LINUX(close) {
    (void)thread;
    if (!process) return -EINVAL; 

    int fd = regs->arg1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    process->fds[fd].vnode = NULL;
    process->fds[fd].offset = 0;
    process->fds[fd].flags = 0;
    process->fds[fd].used = false;

    return 0;
}

SYSCALL_DEFINE_LINUX(openat) {
    (void)thread;
    if (!process) return -EINVAL; 

    int dfd = (int)regs->arg1;  
    const char* path = (const char*)regs->arg2;
    int flags = (int)regs->arg3;
    (void)dfd;

    if (!path) return -EFAULT;

    int fd = -1;
    for (int i = 3; i < MAX_FILES_PER_PROCESS; i++) {
        if (!process->fds[i].used) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -EMFILE;
    }

    vnode_t* vnode = NULL;
    int res = vfs_open(path, flags, &vnode);
    dprintk("Syscall", "Accessing through openat %s, code %d", path, res);
    if (res != 0) {
        return res;
    }

    process->fds[fd].vnode = vnode;
    process->fds[fd].offset = (flags & O_APPEND) ? vnode->size : 0;
    process->fds[fd].flags = flags;
    process->fds[fd].used = true;

    return fd;
}

SYSCALL_DEFINE_LINUX(poll) {
    (void)thread;

    struct pollfd *fds = (struct pollfd*)regs->arg1;
    unsigned long nfds = regs->arg2;

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

SYSCALL_DEFINE_LINUX(brk) {
    (void)thread;

    uint64_t requested_break = regs->arg1;

    if (!process) {
        return -ENOSYS;
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

SYSCALL_DEFINE_LINUX(arch_prctl) {
    (void)process; (void)thread;

    uint64_t code = regs->arg1;
    uint64_t addr = regs->arg2;

    dprintk("Syscall", "Calling arch_ptrctl with code %d addr %p", code, addr);

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
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
        return -ENOSYS;
    }
#else 
    (void)code; (void)addr;
    return 0;
#endif
}

SYSCALL_DEFINE_LINUX(exit) {
    (void)regs; (void)process; (void)thread;
    thread_exit();
    return 0; 
}

SYSCALL_DEFINE_LINUX(stub_unimplemented) {
    (void)regs; (void)process; (void)thread;
    printk("Syscall", "fixme: syscall %d reached syscall_stub_unimplemented", regs->syscall_no);
    return 0;
}

SYSCALL_DEFINE_LINUX(uname) {
    (void)process; (void)thread;

    struct utsname* ptr = (struct utsname*)regs->arg1;

    strcpy(ptr->sysname, "Utopia");
    strcpy(ptr->nodename, "example-computer-lol");
    strcpy(ptr->release, UTOPIA_VERSION);
    strcpy(ptr->version, "(no build information)");
    strcpy(ptr->machine, "x86_64");
    strcpy(ptr->domainname, "");

    return 0;
}

SYSCALL_DEFINE_LINUX(mmap) {
    (void)thread;
    if (!process) return (uint64_t)-1;
    
    uint64_t addr   = regs->arg1;
    uint64_t length = regs->arg2;
    uint64_t prot   = regs->arg3;
    uint64_t flags  = regs->arg4;
    int64_t  fd     = (int64_t)regs->arg6;
    uint64_t offset = regs->arg5;

    if (length == 0) return 0;

    uint64_t alloc_size = page_align_up(length);
    uint64_t pages_count = alloc_size / 0x1000;

    uint64_t target_vaddr;
    if (addr == 0) {
        target_vaddr = process->mmap_current;
        process->mmap_current += alloc_size;
    } else {
        target_vaddr = page_align_up(addr);
    }

    uint64_t pflags = VMF_USER;
    if (prot & 0x2) pflags |= VMF_WRITE;
    if (prot & 0x1) pflags |= VMF_EXEC;

    bool anonymous = (flags & MAP_ANONYMOUS) || fd < 0;
    vnode_t *node = NULL;
    if (!anonymous) {
        if (fd < 0 || !process->fds[fd].vnode) return (uint64_t)-1; // -EBADF
        node = process->fds[fd].vnode;
    }

    void *virt_block = page_alloc(pages_count);
    if (!virt_block) return 0;

    uint64_t phys_base = hhdm_virt_to_phys(virt_block);
    memset(virt_block, 0, alloc_size);

    if (node) {
        if (offset < node->size) {
            uint64_t want = node->size - offset;
            if (want > alloc_size) want = alloc_size;
            
            uint64_t bytes_read = 0;
            node->ops->read(node, virt_block, want, offset, &bytes_read);
        }
    }

    for (uint64_t i = 0; i < pages_count; i++) {
        uint64_t vaddr = target_vaddr + (i * 0x1000);
        uint64_t phys  = phys_base + (i * 0x1000);

        if (map_page_4k(process->page_table, vaddr, phys, pflags) != 0) {
            free(virt_block); 
            return 0;
        }
    }

    return target_vaddr;
}

SYSCALL_DEFINE_LINUX(access) {
    (void)thread;
    if (!process) return -EINVAL; 

    const char* pathname = (const char*)regs->arg1;
    int mode = (int)regs->arg2;

    if (!pathname) return -EFAULT; 

    dprintk("Syscall", "Accessing %s", pathname);

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

    if ((mode & R_OK) && !(st_mode & 0444)) return -EACCES;
    if ((mode & W_OK) && !(st_mode & 0222)) return -EACCES;
    if ((mode & X_OK) && !(st_mode & 0111)) return -EACCES;

    return 0;
}

SYSCALL_DEFINE_LINUX(newfstatat) {
    (void)thread;
    if (!process) return -EINVAL;

    int dfd = (int)regs->arg1;
    const char* filename = (const char*)regs->arg2;
    struct stat* statbuf = (struct stat*)regs->arg3;
    int flag = (int)regs->arg4;
    
    (void)dfd;  
    (void)flag;

    if (!filename || !statbuf) return -EFAULT; 

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

SYSCALL_DEFINE_LINUX(getdents64) {
    (void)thread;

    if (!process)
        return -EINVAL;

    int fd = (int)regs->arg1;
    void* user_buf = (void*)regs->arg2;
    size_t count = (size_t)regs->arg3;

    if (!user_buf) return -EFAULT;
    if (count == 0) return 0;

    vnode_t* dir = process->fds[fd].vnode;
    if (!dir) return -EBADF;
    if (dir->type != VNODE_TYPE_DIR) return -ENOTDIR;
    if (!dir->ops || !dir->ops->readdir) return -ENOTDIR;

    size_t written = 0;
    uint64_t index = process->fds[fd].offset;

    while (written < count) {
        vfs_dirent_t entry;

        int res = dir->ops->readdir(dir, index, &entry);

        if (res == -ENOENT)
            break;

        if (res < 0) 
            break;

        size_t name_len = strlen(entry.name);

        size_t reclen =
            (sizeof(uint64_t) +
             sizeof(int64_t) +
             sizeof(uint16_t) +
             sizeof(uint8_t) +
             name_len + 1 + 7) & ~7ULL;

        if (reclen > count - written) break;

        struct linux_dirent64 *d = (struct linux_dirent64 *)((uint8_t *)user_buf + written);

        d->d_ino = entry.ino;
        d->d_off = (int64_t)(index + 1);
        d->d_reclen = (unsigned short)reclen;
        d->d_type = entry.type;
        memcpy(d->d_name, entry.name, name_len);
        d->d_name[name_len] = '\0';

        size_t used =
            sizeof(uint64_t) +
            sizeof(int64_t) +
            sizeof(uint16_t) +
            sizeof(uint8_t) +
            name_len + 1;

        if (reclen > used) memset((uint8_t *)d + used, 0, reclen - used);

        written += reclen;
        index++;
    }

    process->fds[fd].offset = index;

    return (int)written; 
}

SYSCALL_DEFINE_LINUX(set_tid_address) {
    (void) process;

    if ((int32_t*)regs->arg1 != NULL) {
        *((int32_t*)regs->arg1) = thread->id;
    }

    return thread->id;
}

SYSCALL_DEFINE_LINUX(getrandom) {
    (void)thread; (void)process;

    uint8_t* buf = (uint8_t*)regs->arg1;
    size_t count = regs->arg2;

    if (!buf) return -EFAULT;
    
    for (size_t i = 0; i < count; i++) {
        buf[i] = (char)(i ^ 0x5A); 
    }
    return count; 
}

static const syscall_fn_t syscall_linux_table[] = {
    [0]   = syscall_linux_read,
    [1]   = syscall_linux_write,
    [2]   = syscall_linux_open,
    [3]   = syscall_linux_close,
    [5]   = syscall_linux_fstat,
    [7]   = syscall_linux_poll,
    [8]   = syscall_linux_lseek,
    [9]   = syscall_linux_mmap,
    [10]  = syscall_linux_mprotect, 
    [11]  = syscall_linux_munmap,
    [12]  = syscall_linux_brk,
    [13]  = syscall_linux_stub_unimplemented,
    [16]  = syscall_linux_stub_unimplemented,
    [17]  = syscall_linux_pread64,
    [20]  = syscall_linux_writev,
    [21]  = syscall_linux_access,
    [60]  = syscall_linux_exit,
    [63]  = syscall_linux_uname,
    [79]  = syscall_linux_getcwd,
    [102] = syscall_linux_stub_unimplemented, // that is indeed correct. let me explain.
                                              // this returns 0 and 0 means root
    [158] = syscall_linux_arch_prctl,
    [202] = syscall_linux_stub_unimplemented,
    [217] = syscall_linux_getdents64,
    [218] = syscall_linux_set_tid_address,
    [231] = syscall_linux_exit,
    [257] = syscall_linux_openat,
    [262] = syscall_linux_newfstatat,
    [273] = syscall_linux_stub_unimplemented,
    [318] = syscall_linux_getrandom
};

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
static syscall_regs_t syscall_linux_to_sregs(registers_t* regs) {
    return (syscall_regs_t){
        .syscall_no = regs->rax,
        .arg1 = regs->rdi, .arg2 = regs->rsi, .arg3 = regs->rdx,
        .arg4 = regs->r10, .arg5 = regs->r9, .arg6 = regs->r8
    };
};

static void syscall_set_return_val(int64_t val, registers_t* regs) {
    regs->rax_i = val;
}

SYSCALL_ABI_DEFINE(linux, syscall_linux_table, syscall_linux_to_sregs, syscall_set_return_val, -ENOSYS);
#else
SYSCALL_ABI_DEFINE(linux, syscall_linux_table, NULL, NULL, -ENOSYS);
#endif
