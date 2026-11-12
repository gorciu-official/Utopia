#include <config.h>
#include <constants.h>

#include <fs/filesystem.h>
#include <drivers/keyboard.h>
#include <drivers/framebuffer.h>

#include <lib/screen.h>

#include "../common.h"
#include "common.h"

extern bool keyboard_should_putchar;

#define USER_VA_LIMIT (1ULL << USER_VA_BITS)

#ifdef CONFIG_SYSCALL_ABI_LINUX

static size_t do_read(process_t* process, int fd, char* buf, size_t count, uintptr_t offset, bool update_offset) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    if (fd == 0) {
        return keyboard_read(buf, count);
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

static uintptr_t do_write(process_t* process, void* data, uintptr_t len, int fd) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    if (fd == 1 || fd == 2) {
        user_print(data, len);
        return len;
    }

    vnode_t* vnode = process->fds[fd].vnode;
    if (!vnode) return -EBADF; 

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

SYSCALL_DEFINE_LINUX(read) {
    (void)thread;
    int fd = regs->arg1;
    char* buf = (char*)regs->arg2;
    size_t count = regs->arg3;

    return SCRC(do_read(process, fd, buf, count, process->fds[fd].offset, true));
}

SYSCALL_DEFINE_LINUX(pread64) {
    (void)thread;
    int fd = regs->arg1;
    char* buf = (char*)regs->arg2;
    size_t count = regs->arg3;
    uintptr_t offset = regs->arg4;

    return SCRC(do_read(process, fd, buf, count, offset, false));
}

SYSCALL_DEFINE_LINUX(write) {
    (void)thread;
    return SCRC(do_write(process, (void*)regs->arg2, regs->arg3, regs->arg1));
}

SYSCALL_DEFINE_LINUX(writev) {
    (void)thread;
    if (!process) return SCRC(-1);

    int fd = regs->arg1;
    iovec_t* iov = (iovec_t*)regs->arg2;
    int iovcnt = regs->arg3;

    size_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        size_t ret = do_write(process, iov[i].iov_base, iov[i].iov_len, fd);
        if (ret == (uintptr_t)-EBADF) {
            if (total == 0) return SCRC(-EBADF);
            break;
        }
        total += ret;

        if ((size_t)ret < iov[i].iov_len) {
            break;
        }
    }

    return SCRC(total);
}

SYSCALL_DEFINE_LINUX(ioctl) {
    (void)thread;

    if (regs->arg2 == TCSETS) {
        struct termios* tos = (struct termios*)regs->arg3;
        keyboard_should_putchar = !!(tos->c_lflag & TERMIOS_ECHO);
    } else if (regs->arg2 == TCGETS) {
        struct termios* tos = (struct termios*)regs->arg3;
        tos->c_lflag = 0;
    } else if (regs->arg2 == TIOCGPGRP) {
        int* pgrp = (int*)regs->arg3;
        if (!pgrp || !process) return SCRC(-EFAULT);
        *pgrp = (int)process->pid;
    } else if (regs->arg2 == TIOCSPGRP) {
    } else if (regs->arg2 == TIOCGWINSZ) {
        struct winsize* winsize = (struct winsize*)regs->arg3;
        winsize->ws_col = framebuffer_get_height() / framebuffer_font_height();
        winsize->ws_row = framebuffer_get_width() / framebuffer_font_height();
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(mkdirat) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    // dirfd is ignored like in openat: relative paths are resolved from /
    const char* path = (const char*)regs->arg2;
    if (!path || (uint64_t)(uintptr_t)path >= USER_VA_LIMIT) return SCRC(-EFAULT);

    int res = vfs_mkdir(path);
    dprintk("Syscall", "mkdirat %s, code %d", path, res);
    return SCRC(res);
}

SYSCALL_DEFINE_LINUX(prlimit64) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int pid = (int)regs->arg1;
    uint32_t resource = (uint32_t)regs->arg2;
    uint64_t new_limit = regs->arg3;
    uint64_t old_limit = regs->arg4;

    if (pid != 0 && (uint32_t)pid != process->pid) return SCRC(-ESRCH);
    if (resource >= RLIM_NLIMITS) return SCRC(-EINVAL);
    if (new_limit && new_limit + sizeof(struct rlimit64) > USER_VA_LIMIT) return SCRC(-EFAULT);
    if (old_limit && old_limit + sizeof(struct rlimit64) > USER_VA_LIMIT) return SCRC(-EFAULT);

    // limits are not enforced, so setting one is accepted and forgotten
    if (old_limit) {
        struct rlimit64* out = (struct rlimit64*)old_limit;
        out->rlim_cur = RLIM_INFINITY;
        out->rlim_max = RLIM_INFINITY;
        if (resource == RLIMIT_NOFILE) {
            out->rlim_cur = MAX_FILES_PER_PROCESS;
            out->rlim_max = MAX_FILES_PER_PROCESS;
        }
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(close_range) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    uint64_t first = regs->arg1;
    uint64_t last = regs->arg2;
    uint64_t flags = regs->arg3;

    if (flags & ~(uint64_t)(CLOSE_RANGE_UNSHARE | CLOSE_RANGE_CLOEXEC)) return SCRC(-EINVAL);
    if (first > last) return SCRC(-EINVAL);

    // file tables are never shared, so UNSHARE changes nothing, and there is
    // no close-on-exec tracking, so CLOEXEC has nothing to mark
    if (flags & CLOSE_RANGE_CLOEXEC) return SCRC(0);

    if (last >= MAX_FILES_PER_PROCESS) last = MAX_FILES_PER_PROCESS - 1;

    for (uint64_t fd = first; fd <= last; fd++) {
        if (!process->fds[fd].used) continue;
        process->fds[fd].vnode = NULL;
        process->fds[fd].offset = 0;
        process->fds[fd].flags = 0;
        process->fds[fd].used = false;
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(sync) {
    // there is no writable block storage to flush
    (void)regs; (void)process; (void)thread;
    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(mount) {
    (void)regs; (void)process; (void)thread;
    return SCRC(0);
}

#endif
