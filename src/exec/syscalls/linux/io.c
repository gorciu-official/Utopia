#include <config.h>
#include <constants.h>

#include <fs/filesystem.h>
#include <drivers/keyboard.h>
#include <drivers/framebuffer.h>

#include <lib/screen.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <mm/user.h>

#include "../common.h"
#include "common.h"

extern bool keyboard_should_putchar;

#define USER_VA_LIMIT (1ULL << USER_VA_BITS)

#define MAX_SYSCALL_IO_BYTES (1 * 1024 * 1024)

#define LINUX_KERNEL_TERMIOS_SIZE 36

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
    if (!process) return SCRC(-EINVAL);

    int fd = regs->arg1;
    void* ubuf = (void*)regs->arg2;
    size_t count = regs->arg3;

    if (count == 0) return SCRC(0);
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return SCRC(-EBADF);
    if (count > MAX_SYSCALL_IO_BYTES) return SCRC(-EINVAL);
    if (!ubuf || !user_range_ok((uint64_t)ubuf, count)) return SCRC(-EFAULT);

    char* kbuf = malloc(count);
    if (!kbuf) return SCRC(-ENOMEM);

    size_t ret = do_read(process, fd, kbuf, count, process->fds[fd].offset, true);
    if (ret != (size_t)-1 && (int64_t)ret > 0) {
        if (copy_to_user(process, ubuf, kbuf, ret) != 0) {
            free(kbuf);
            return SCRC(-EFAULT);
        }
    }

    free(kbuf);
    return SCRC(ret);
}

SYSCALL_DEFINE_LINUX(pread64) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int fd = regs->arg1;
    void* ubuf = (void*)regs->arg2;
    size_t count = regs->arg3;
    uintptr_t offset = regs->arg4;

    if (count == 0) return SCRC(0);
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return SCRC(-EBADF);
    if (count > MAX_SYSCALL_IO_BYTES) return SCRC(-EINVAL);
    if (!ubuf || !user_range_ok((uint64_t)ubuf, count)) return SCRC(-EFAULT);

    char* kbuf = malloc(count);
    if (!kbuf) return SCRC(-ENOMEM);

    size_t ret = do_read(process, fd, kbuf, count, offset, false);
    if (ret != (size_t)-1 && (int64_t)ret > 0) {
        if (copy_to_user(process, ubuf, kbuf, ret) != 0) {
            free(kbuf);
            return SCRC(-EFAULT);
        }
    }

    free(kbuf);
    return SCRC(ret);
}

SYSCALL_DEFINE_LINUX(write) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int fd = regs->arg1;
    void* ubuf = (void*)regs->arg2;
    size_t count = regs->arg3;

    if (count == 0) return SCRC(0);
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return SCRC(-EBADF);
    if (count > MAX_SYSCALL_IO_BYTES) return SCRC(-EINVAL);
    if (!ubuf || !user_range_ok((uint64_t)ubuf, count)) return SCRC(-EFAULT);

    char* kbuf = malloc(count);
    if (!kbuf) return SCRC(-ENOMEM);

    if (copy_from_user(process, kbuf, ubuf, count) != 0) {
        free(kbuf);
        return SCRC(-EFAULT);
    }

    uintptr_t ret = do_write(process, kbuf, count, fd);
    free(kbuf);
    return SCRC(ret);
}

SYSCALL_DEFINE_LINUX(writev) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int fd = regs->arg1;
    void* uiov = (void*)regs->arg2;
    int iovcnt = (int)regs->arg3;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used)
        return SCRC(-EBADF);
    if (iovcnt <= 0 || iovcnt > 1024) return SCRC(-EINVAL);
    if (!uiov || !user_range_ok((uint64_t)uiov, (size_t)iovcnt * sizeof(iovec_t)))
        return SCRC(-EFAULT);

    iovec_t* iov = malloc((size_t)iovcnt * sizeof(iovec_t));
    if (!iov) return SCRC(-ENOMEM);

    if (copy_from_user(process, iov, uiov, (size_t)iovcnt * sizeof(iovec_t)) != 0) {
        free(iov);
        return SCRC(-EFAULT);
    }

    size_t total = 0;

    for (int i = 0; i < iovcnt; i++) {
        void* ubase = iov[i].iov_base;
        size_t len  = iov[i].iov_len;

        if (len == 0) continue;
        if (len > MAX_SYSCALL_IO_BYTES) {
            free(iov);
            return SCRC(-EINVAL);
        }
        if (!ubase || !user_range_ok((uint64_t)ubase, len)) {
            free(iov);
            return SCRC(-EFAULT);
        }

        char* kbuf = malloc(len);
        if (!kbuf) {
            free(iov);
            return SCRC(-ENOMEM);
        }

        if (copy_from_user(process, kbuf, ubase, len) != 0) {
            free(kbuf);
            free(iov);
            return SCRC(-EFAULT);
        }

        size_t ret = do_write(process, kbuf, len, fd);
        free(kbuf);

        if (ret == (uintptr_t)-EBADF) {
            if (total == 0) {
                free(iov);
                return SCRC(-EBADF);
            }
            break;
        }
        total += ret;

        if (ret < len) break;
    }

    free(iov);
    return SCRC(total);
}

SYSCALL_DEFINE_LINUX(ioctl) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    unsigned long cmd = regs->arg2;
    void* arg = (void*)regs->arg3;

    if (cmd == TCSETS) {
        struct termios tos;
        memset(&tos, 0, sizeof(tos));
        if (!arg || copy_from_user(process, &tos, arg, LINUX_KERNEL_TERMIOS_SIZE) != 0)
            return SCRC(-EFAULT);
        keyboard_should_putchar = !!(tos.c_lflag & TERMIOS_ECHO);
    } else if (cmd == TCGETS) {
        struct termios tos = {0};
        if (!arg || copy_to_user(process, arg, &tos, LINUX_KERNEL_TERMIOS_SIZE) != 0)
            return SCRC(-EFAULT);
    } else if (cmd == TIOCGPGRP) {
        int pgrp = (int)process->pid;
        if (!arg || copy_to_user(process, arg, &pgrp, sizeof(pgrp)) != 0)
            return SCRC(-EFAULT);
    } else if (cmd == TIOCSPGRP) {
        /* no-op for now */
    } else if (cmd == TIOCGWINSZ) {
        struct winsize winsize = {0};
        winsize.ws_col = framebuffer_get_height() / framebuffer_font_height();
        winsize.ws_row = framebuffer_get_width() / framebuffer_font_height();
        if (!arg || copy_to_user(process, arg, &winsize, sizeof(winsize)) != 0)
            return SCRC(-EFAULT);
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(mkdirat) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int dfd = (int)regs->arg1;
    const char* upath = (const char*)regs->arg2;

    char path[VFS_PATH_MAX];
    if (!upath) return SCRC(-EFAULT);
    int plen = strncpy_from_user(process, path, upath, VFS_PATH_MAX);
    if (plen < 0) return SCRC(plen == -UACCESS_ENAMETOOLONG ? -ENAMETOOLONG : -EFAULT);

    char resolved[VFS_PATH_MAX];
    int res = process_resolve_at(process, dfd, path, resolved);
    if (res != 0) return SCRC(res);

    res = vfs_mkdir(resolved);
    dprintk("Syscall", "mkdirat %s (%s), code %d", path, resolved, res);
    return SCRC(res);
}

SYSCALL_DEFINE_LINUX(prlimit64) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int pid = (int)regs->arg1;
    uint32_t resource = (uint32_t)regs->arg2;
    void* unew = (void*)regs->arg3;
    void* uold = (void*)regs->arg4;

    if (pid != 0 && (uint32_t)pid != process->pid) return SCRC(-ESRCH);
    if (resource >= RLIM_NLIMITS) return SCRC(-EINVAL);

    if (uold) {
        struct rlimit64 out = {
            .rlim_cur = RLIM_INFINITY,
            .rlim_max = RLIM_INFINITY,
        };
        if (resource == RLIMIT_NOFILE) {
            out.rlim_cur = MAX_FILES_PER_PROCESS;
            out.rlim_max = MAX_FILES_PER_PROCESS;
        }
        if (copy_to_user(process, uold, &out, sizeof(out)) != 0)
            return SCRC(-EFAULT);
    }

    (void)unew;

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

    if (flags & CLOSE_RANGE_CLOEXEC) return SCRC(0);

    if (last >= MAX_FILES_PER_PROCESS) last = MAX_FILES_PER_PROCESS - 1;

    for (uint64_t fd = first; fd <= last; fd++) {
        if (!process->fds[fd].used) continue;
        process->fds[fd].vnode = NULL;
        process->fds[fd].offset = 0;
        process->fds[fd].flags = 0;
        process->fds[fd].used = false;
        process->fds[fd].path[0] = '\0';
    }

    return SCRC(0);
}

SYSCALL_DEFINE_LINUX(sync) {
    (void)regs; (void)process; (void)thread;
    return SCRC(0);
}

#define MOUNT_PATH_MAX 256
#define MOUNT_TYPE_MAX 64

#define MS_MGC_VAL 0xC0ED0000UL
#define MS_MGC_MSK 0xFFFF0000UL

static int copy_user_string(process_t* process, const char* user, char* out, size_t max) {
    if (!user || max == 0) return -EFAULT;

    int len = strncpy_from_user(process, out, user, max);
    if (len == -UACCESS_ENAMETOOLONG) return -ENAMETOOLONG;
    if (len < 0) return -EFAULT;
    return 0;
}

static int resolve_user_path(process_t* process, const char* raw, char* out) {
    char norm[VFS_PATH_MAX];

    int err = vfs_resolve_path(process->cwd, raw, norm);
    if (err) return err;

    int fd = procfs_parse_fd_path(norm, process->pid);
    if (fd < 0) {
        memcpy(out, norm, strlen(norm) + 1);
        return 0;
    }

    if (fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used || !process->fds[fd].path[0]) {
        return -ENOENT;
    }

    memcpy(out, process->fds[fd].path, strlen(process->fds[fd].path) + 1);
    return 0;
}

SYSCALL_DEFINE_LINUX(mount) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    const char* user_source = (const char*)regs->arg1;
    const char* user_target = (const char*)regs->arg2;
    const char* user_type   = (const char*)regs->arg3;
    unsigned long flags     = (unsigned long)regs->arg4;

    char source[MOUNT_PATH_MAX];
    char target[MOUNT_PATH_MAX];
    char type[MOUNT_TYPE_MAX];
    char resolved[VFS_PATH_MAX];

    int err = copy_user_string(process, user_target, target, sizeof(target));
    if (err) return SCRC(err);

    err = resolve_user_path(process, target, resolved);
    if (err) return SCRC(err);
    memcpy(target, resolved, strlen(resolved) + 1);

    source[0] = '\0';
    if (user_source) {
        err = copy_user_string(process, user_source, source, sizeof(source));
        if (err) return SCRC(err);
    }

    type[0] = '\0';
    if (user_type) {
        err = copy_user_string(process, user_type, type, sizeof(type));
        if (err) return SCRC(err);
    }

    if ((flags & MS_MGC_MSK) == MS_MGC_VAL) flags &= ~MS_MGC_MSK;

    if ((flags & VFS_MS_BIND) && !(flags & VFS_MS_REMOUNT) && user_source) {
        err = resolve_user_path(process, source, resolved);
        if (err) return SCRC(err);
        memcpy(source, resolved, strlen(resolved) + 1);
    }

    int res;

    if (flags & VFS_MS_REMOUNT) {
        res = vfs_remount(target, flags);
    } else if (flags & VFS_MS_BIND) {
        res = user_source ? vfs_bind_mount(source, target) : -EINVAL;
    } else if (flags & (VFS_MS_SHARED | VFS_MS_PRIVATE | VFS_MS_SLAVE | VFS_MS_UNBINDABLE)) {
        vnode_t* node = NULL;
        res = vfs_lookup(target, &node) == 0 ? 0 : -ENOENT;
    } else if (flags & VFS_MS_MOVE) {
        res = -ENOSYS;
    } else {
        res = user_type ? vfs_mount(type, user_source ? source : NULL, target, flags) : -EINVAL;
    }

    dprintk("Syscall", "mount %s on %s, code %d", type, target, res);
    return SCRC(res);
}

SYSCALL_DEFINE_LINUX(umount2) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    char target[MOUNT_PATH_MAX];
    char resolved[VFS_PATH_MAX];

    int err = copy_user_string(process, (const char*)regs->arg1, target, sizeof(target));
    if (err) return SCRC(err);

    err = resolve_user_path(process, target, resolved);
    if (err) return SCRC(err);

    int res = vfs_umount(resolved, (int)regs->arg2);
    dprintk("Syscall", "umount2 %s, code %d", target, res);
    return SCRC(res);
}

SYSCALL_DEFINE_LINUX(chdir) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    char raw[VFS_PATH_MAX];
    char resolved[VFS_PATH_MAX];

    int err = copy_user_string(process, (const char*)regs->arg1, raw, sizeof(raw));
    if (err) return SCRC(err);

    err = process_resolve_at(process, VFS_AT_FDCWD, raw, resolved);
    if (err) return SCRC(err);

    vnode_t* node = NULL;
    if (vfs_lookup(resolved, &node) != 0 || !node) return SCRC(-ENOENT);
    if (node->type != VNODE_TYPE_DIR) return SCRC(-ENOTDIR);

    err = process_chdir(process, resolved);
    dprintk("Syscall", "chdir %s -> %s, code %d", raw, resolved, err);
    return SCRC(err);
}

SYSCALL_DEFINE_LINUX(fchdir) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int fd = (int)regs->arg1;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !process->fds[fd].used) {
        return SCRC(-EBADF);
    }

    file_desc_t* desc = &process->fds[fd];
    if (!desc->vnode || desc->vnode->type != VNODE_TYPE_DIR) return SCRC(-ENOTDIR);
    if (!desc->path[0]) return SCRC(-ENOENT);

    int err = process_chdir(process, desc->path);
    dprintk("Syscall", "fchdir %d -> %s, code %d", fd, desc->path, err);
    return SCRC(err);
}

#endif
