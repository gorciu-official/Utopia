#include <config.h>
#include <constants.h>

#include <drivers/filesystem.h>
#include <drivers/framebuffer.h>

#include <lib/screen.h>

#include "../common.h"
#include "common.h"

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#include <drivers/arch-specific/ps2.h>

extern bool keyboard_should_putchar;
#endif

#ifdef CONFIG_SYSCALL_ABI_LINUX

static size_t do_read(process_t* process, int fd, char* buf, size_t count, uintptr_t offset, bool update_offset) {
    if (!process) return -1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return -EBADF; 
    }

    if (fd == 0) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        return ps2_read(buf, count);
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
    (void)process; (void)thread;

    if (regs->arg2 == TCSETS) {
        struct termios* tos = (struct termios*)regs->arg3;
        keyboard_should_putchar = !!(tos->c_lflag & TERMIOS_ECHO);
    } else if (regs->arg2 == TIOCGWINSZ) {
        struct winsize* winsize = (struct winsize*)regs->arg3;
        winsize->ws_col = framebuffer_get_height() / framebuffer_font_height();
        winsize->ws_row = framebuffer_get_width() / framebuffer_font_height();
    }

    return SCRC(0);
}

#endif
