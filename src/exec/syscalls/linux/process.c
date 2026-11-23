#include <types.h>
#include <config.h>
#include <mm/pmm.h>
#include <mm/user.h>

#include <exec/loaders/elf.h>

#include <fs/filesystem.h>

#include <lib/string.h>

#include "../common.h"
#include "common.h"

#ifdef CONFIG_SYSCALL_ABI_LINUX

#define EXEC_MAX_ARGS       1024
#define EXEC_MAX_PATH       4096
#define EXEC_MAX_ARG_BYTES  (256 * 1024)
#define USER_VA_LIMIT       (1ULL << USER_VA_BITS)

static void exec_free_strvec(char** vec, int count) {
    if (!vec) return;
    for (int i = 0; i < count; i++) free(vec[i]);
    free(vec);
}

static int exec_copy_strvec(process_t* proc, char* const* user_vec,
                            char*** out_vec, int* out_count, size_t* budget) {
    *out_vec = NULL;
    *out_count = 0;

    if (!user_vec) return 0;
    if (!user_range_ok((uint64_t)(uintptr_t)user_vec, sizeof(char*))) return -EFAULT;

    int count = 0;
    while (count <= EXEC_MAX_ARGS) {
        char* ptr = NULL;
        if (copy_from_user(proc, &ptr, &user_vec[count], sizeof(ptr)) != 0)
            return -EFAULT;
        if (!ptr) break;
        count++;
    }
    if (count > EXEC_MAX_ARGS) return -7;

    char** vec = (char**)calloc((size_t)count + 1, sizeof(char*));
    if (!vec) return -ENOMEM;

    for (int i = 0; i < count; i++) {
        char* src = NULL;
        if (copy_from_user(proc, &src, &user_vec[i], sizeof(src)) != 0) {
            exec_free_strvec(vec, i);
            return -EFAULT;
        }
        if (!src || !user_range_ok((uint64_t)(uintptr_t)src, 1)) {
            exec_free_strvec(vec, i);
            return -EFAULT;
        }

        size_t max = *budget < EXEC_MAX_ARG_BYTES ? *budget : EXEC_MAX_ARG_BYTES;
        if (max == 0) { exec_free_strvec(vec, i); return -7; }
        char* kstr = (char*)malloc(max);
        if (!kstr) { exec_free_strvec(vec, i); return -ENOMEM; }
        int slen = strncpy_from_user(proc, kstr, src, max);
        if (slen < 0) {
            free(kstr);
            exec_free_strvec(vec, i);
            return slen == -UACCESS_ENAMETOOLONG ? -7 : -EFAULT;
        }
        size_t len = (size_t)slen;
        *budget -= len;

        char* exact = (char*)malloc(len);
        if (exact) { memcpy(exact, kstr, len); free(kstr); kstr = exact; }
        vec[i] = kstr;
    }

    *out_vec = vec;
    *out_count = count;
    return 0;
}

SYSCALL_DEFINE_LINUX(execve) {
    if (!process || !thread || !regs->frame) return SCRC(-EINVAL);

    const char* user_path = (const char*)regs->arg1;
    if (!user_path) return SCRC(-EFAULT);

    char path_buf[EXEC_MAX_PATH];
    int plen = strncpy_from_user(process, path_buf, user_path, EXEC_MAX_PATH);
    if (plen < 0) return SCRC(plen == -UACCESS_ENAMETOOLONG ? -ENAMETOOLONG : -EFAULT);
    size_t path_len = (size_t)plen;

    char* path = (char*)malloc(path_len);
    if (!path) return SCRC(-ENOMEM);
    memcpy(path, path_buf, path_len);

    char** argv = NULL; int argc = 0;
    char** envp = NULL; int envc = 0;
    uint8_t* image = NULL;
    int err = 0;
    vnode_t* node = NULL;
    uint64_t size = 0, total = 0;
    create_elf_process_request_t req;
    elf_prepared_t prepared;
    const char* base = NULL;

    size_t budget = EXEC_MAX_ARG_BYTES;
    err = exec_copy_strvec(process, (char* const*)regs->arg2, &argv, &argc, &budget);
    if (err) goto out;
    err = exec_copy_strvec(process, (char* const*)regs->arg3, &envp, &envc, &budget);
    if (err) goto out;

    if (argc == 0) {
        argv = (char**)calloc(2, sizeof(char*));
        if (!argv) { 
            err = -ENOMEM; 
            goto out; 
        }
        argv[0] = path;   
        argc = 1;
    }

    char resolved[VFS_PATH_MAX];
    err = process_resolve_at(process, VFS_AT_FDCWD, path, resolved);
    if (err) goto out;

    vfs_lookup(resolved, &node);
    if (!node) { err = -ENOENT; goto out; }
    if (node->type == VNODE_TYPE_DIR) {
        err = -EACCES; 
        goto out;
    } 
    if (!node->ops || !node->ops->read) { 
        err = -ENOEXEC; goto out; 
    }

    size = node->size;
    if (size < 4) { 
        err = -ENOEXEC; goto out; 
    }

    image = (uint8_t*)malloc(size);
    if (!image) { 
        err = -ENOMEM; goto out; 
    }

    while (total < size) {
        uint64_t got = 0;
        if (node->ops->read(node, image + total, size - total, total, &got) != 0 || got == 0) break;
        total += got;
    }
    if (total != size) { 
        err = -ENOEXEC; 
        goto out; 
    }

    req = (create_elf_process_request_t){
        .syscall_conv = process->syscall_conv,
        .image = { .data = image, .size = size },
        .args  = { .envp = envp, .argc = argc, .argv = argv }
    };

    if (elf_prepare(&req, &prepared) != 0) { 
        err = -ENOEXEC; goto out; 
    }

    base = path;
    for (const char* c = path; *c; c++) if (*c == '/' && c[1]) base = c + 1;

    process_replace(process, thread, regs->frame, &prepared, base);

out:
    if (argv && argc == 1 && argv[0] == path) argv[0] = NULL;
    exec_free_strvec(argv, argc);
    exec_free_strvec(envp, envc);
    if (image) free(image);
    free(path);
    return SCRC(err);
}

static syscall_result_t do_clone(syscall_regs_t* regs, process_t* process, thread_t* thread, uint64_t flags, uint64_t child_stack, uint64_t ptid, uint64_t ctid, uint64_t tls) {
    if (!process || !thread || !regs->frame) return SCRC(-EINVAL);

    // yes, we do not handle common address space so basically it's kinda hope
    // it works, but should work for a typical "clone -> execve" pattern
    if (
        (flags & (CLONE_VM | CLONE_VFORK)) == (CLONE_VM | CLONE_VFORK) &&
        !(flags & (CLONE_THREAD | CLONE_SIGHAND))
    )
        flags &= ~(uint64_t)CLONE_VM;

    if (flags & CLONE_UNSUPPORTED) return SCRC(-EINVAL);

    process_clone_opts_t opts = {
        .child_stack    = child_stack,
        .set_tls        = (flags & CLONE_SETTLS) != 0,
        .tls            = tls,
        .child_tid_addr = (flags & CLONE_CHILD_SETTID) && ctid < USER_VA_LIMIT ? ctid : 0,
    };

    process_t* child = process_clone(process, thread, regs->frame, &opts);
    if (!child) return SCRC(-ENOMEM);

    if ((flags & CLONE_PARENT_SETTID) && ptid) {
        uint32_t tid = child->main_tid;
        if (copy_to_user(process, (void*)ptid, &tid, sizeof(tid)) != 0) {
        }
    }

    return SCRC(child->pid);
}

SYSCALL_DEFINE_LINUX(fork) {
    return do_clone(regs, process, thread, SIGCHLD, 0, 0, 0, 0);
}

SYSCALL_DEFINE_LINUX(vfork) {
    return do_clone(regs, process, thread, CLONE_VFORK | SIGCHLD, 0, 0, 0, 0);
}

SYSCALL_DEFINE_LINUX(clone) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    return do_clone(regs, process, thread, regs->arg1, regs->arg2, regs->arg3, regs->arg4, regs->arg5);
#else
    return do_clone(regs, process, thread, regs->arg1, regs->arg2, regs->arg3, regs->arg5, regs->arg4);
#endif
}

SYSCALL_DEFINE_LINUX(clone3) {
    clone3_args_t args = {0};

    if (!regs->arg1)
        return SCRC(-EFAULT);

    if (regs->arg2 < 64)
        return SCRC(-EINVAL);

    size_t copy = regs->arg2 > sizeof(args) ? sizeof(args) : regs->arg2;
    if (copy_from_user(process, &args, (void*)regs->arg1, copy) != 0)
        return SCRC(-EFAULT);

    uint64_t flags = args.flags | args.exit_signal;

    uint64_t child_stack = args.stack
        ? args.stack + args.stack_size
        : 0;

    return do_clone(
        regs, process, thread,
        flags, child_stack,
        args.parent_tid, args.child_tid, args.tls
    );
}

SYSCALL_DEFINE_LINUX(wait4) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int pid = (int)regs->arg1;
    void* uwstatus = (void*)regs->arg2;
    int options = (int)regs->arg3;
    void* urusage = (void*)regs->arg4;

    if (options & ~WAIT4_VALID_OPTIONS) return SCRC(-EINVAL);

    int want = pid > 0 ? pid : -1;

    for (;;) {
        uint64_t seq = wait_queue_snapshot(&process->child_wq);

        int status = 0;
        int r = process_wait_child(process, want, &status);
        if (r < 0) return SCRC(-ECHILD);
        if (r > 0) {
            if (uwstatus) {
                if (copy_to_user(process, uwstatus, &status, sizeof(status)) != 0)
                    return SCRC(-EFAULT);
            }
            if (urusage) {
                uint8_t zero[RUSAGE_STRUCT_SIZE] = {0};
                if (copy_to_user(process, urusage, zero, RUSAGE_STRUCT_SIZE) != 0)
                    return SCRC(-EFAULT);
            }
            return SCRC(r);
        }
        if (options & WNOHANG) return SCRC(0);

        scheduler_wait(&process->child_wq, seq);
    }
}

SYSCALL_DEFINE_LINUX(waitid) {
    (void)thread;
    if (!process) return SCRC(-EINVAL);

    int idtype = (int)regs->arg1;
    int id = (int)regs->arg2;
    void* uinfop = (void*)regs->arg3;
    int options = (int)regs->arg4;
    void* urusage = (void*)regs->arg5;

    if (options & ~WAITID_VALID_OPTIONS) return SCRC(-EINVAL);
    if (!(options & (WEXITED | WSTOPPED | WCONTINUED))) return SCRC(-EINVAL);

    int want;
    switch (idtype) {
    case P_ALL:
        want = -1;
        break;
    case P_PID:
        if (id <= 0) return SCRC(-EINVAL);
        want = id;
        break;
    default:
        // process groups and pidfds are not tracked yet
        return SCRC(-EINVAL);
    }

    // only exited children exist for now (no stop/continue tracking)
    bool can_report = (options & WEXITED) != 0;
    bool reap = !(options & WNOWAIT);

    for (;;) {
        uint64_t seq = wait_queue_snapshot(&process->child_wq);

        int status = 0;
        uint32_t child_pid = 0;
        int r = process_wait_child_ex(process, want, &status, &child_pid, reap);
        if (r < 0) return SCRC(-ECHILD);
        if (r > 0 && can_report) {
            if (uinfop) {
                int32_t info[SIGINFO_SIZE / sizeof(int32_t)] = {0};
                info[0] = SIGCHLD;      // si_signo
                info[1] = 0;            // si_errno
                if ((status & 0x7f) == 0) {
                    info[2] = CLD_EXITED;
                    info[6] = (status >> 8) & 0xff;
                } else {
                    info[2] = (status & 0x80) ? CLD_DUMPED : CLD_KILLED;
                    info[6] = status & 0x7f;
                }
                info[4] = (int32_t)child_pid; // si_pid
                info[5] = 0;                  // si_uid
                if (copy_to_user(process, uinfop, info, SIGINFO_SIZE) != 0)
                    return SCRC(-EFAULT);
            }
            if (urusage) {
                uint8_t zero[RUSAGE_STRUCT_SIZE] = {0};
                if (copy_to_user(process, urusage, zero, RUSAGE_STRUCT_SIZE) != 0)
                    return SCRC(-EFAULT);
            }
            return SCRC(0);
        }
        if (options & WNOHANG) {
            if (uinfop) {
                uint8_t zero[SIGINFO_SIZE] = {0};
                if (copy_to_user(process, uinfop, zero, SIGINFO_SIZE) != 0)
                    return SCRC(-EFAULT);
            }
            return SCRC(0);
        }

        scheduler_wait(&process->child_wq, seq);
    }
}

SYSCALL_DEFINE_LINUX(pause) {
    (void)regs; (void)thread;
    if (!process) return SCRC(-EINVAL);

    // there is no signal delivery yet, so this sleeps until something
    // wakes signal_wq (which is what a future signal path should do)
    uint64_t seq = wait_queue_snapshot(&process->signal_wq);
    while (wait_queue_snapshot(&process->signal_wq) == seq)
        scheduler_wait(&process->signal_wq, seq);

    return SCRC(-EINTR);
}

SYSCALL_DEFINE_LINUX(gettid) {
    (void)process; (void)regs;
    return SCRC(thread->id);
}

SYSCALL_DEFINE_LINUX(getpid) {
    (void)thread; (void)regs;
    return SCRC(process->pid);
}

#endif
