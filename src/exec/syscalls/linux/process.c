#include <types.h>
#include <config.h>
#include <mm/pmm.h>

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

static int exec_copy_strvec(char* const* user_vec, char*** out_vec, int* out_count, size_t* budget) {
    *out_vec = NULL;
    *out_count = 0;

    if (!user_vec) return 0;
    if ((uint64_t)(uintptr_t)user_vec >= USER_VA_LIMIT) return -EFAULT;

    int count = 0;
    while (user_vec[count]) {
        if (++count > EXEC_MAX_ARGS) return -7; // E2BIG
    }

    char** vec = (char**)calloc((size_t)count + 1, sizeof(char*));
    if (!vec) return -ENOMEM;

    for (int i = 0; i < count; i++) {
        const char* src = user_vec[i];
        if ((uint64_t)(uintptr_t)src >= USER_VA_LIMIT) { exec_free_strvec(vec, i); return -EFAULT; }

        size_t len = strlen(src) + 1;
        if (len > *budget) { exec_free_strvec(vec, i); return -7; }
        *budget -= len;

        vec[i] = (char*)malloc(len);
        if (!vec[i]) { exec_free_strvec(vec, i); return -ENOMEM; }
        memcpy(vec[i], src, len);
    }

    *out_vec = vec;
    *out_count = count;
    return 0;
}

SYSCALL_DEFINE_LINUX(execve) {
    if (!process || !thread || !regs->frame) return SCRC(-EINVAL);

    const char* user_path = (const char*)regs->arg1;
    if (!user_path || (uint64_t)(uintptr_t)user_path >= USER_VA_LIMIT) return SCRC(-EFAULT);

    size_t path_len = strlen(user_path) + 1;
    if (path_len > EXEC_MAX_PATH) return SCRC(-ENAMETOOLONG);
    char* path = (char*)malloc(path_len);
    if (!path) return SCRC(-ENOMEM);
    memcpy(path, user_path, path_len);

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
    err = exec_copy_strvec((char* const*)regs->arg2, &argv, &argc, &budget);
    if (err) goto out;
    err = exec_copy_strvec((char* const*)regs->arg3, &envp, &envc, &budget);
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

    vfs_lookup(path, &node);
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
    if (flags & CLONE_UNSUPPORTED) return SCRC(-EINVAL);

    process_clone_opts_t opts = {
        .child_stack    = child_stack,
        .set_tls        = (flags & CLONE_SETTLS) != 0,
        .tls            = tls,
        .child_tid_addr = (flags & CLONE_CHILD_SETTID) && ctid < USER_VA_LIMIT ? ctid : 0,
    };

    process_t* child = process_clone(process, thread, regs->frame, &opts);
    if (!child) return SCRC(-ENOMEM);

    if ((flags & CLONE_PARENT_SETTID) && ptid && ptid < USER_VA_LIMIT)
        *(uint32_t*)ptid = child->main_tid;

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

    if (!regs->arg1 || regs->arg1 >= USER_VA_LIMIT)
        return SCRC(-EFAULT);

    if (regs->arg2 < 64)
        return SCRC(-EINVAL);

    if (regs->arg2 > sizeof(args))
        return SCRC(-EINVAL);

    memcpy(&args, (void*)regs->arg1, regs->arg2);

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
    uint64_t wstatus = regs->arg2;
    int options = (int)regs->arg3;
    uint64_t rusage = regs->arg4;

    if (options & ~WAIT4_VALID_OPTIONS) return SCRC(-EINVAL);
    if (wstatus && wstatus + sizeof(int) > USER_VA_LIMIT) return SCRC(-EFAULT);
    if (rusage && rusage + RUSAGE_STRUCT_SIZE > USER_VA_LIMIT) return SCRC(-EFAULT);

    int want = pid > 0 ? pid : -1;

    for (;;) {
        uint64_t seq = wait_queue_snapshot(&process->child_wq);

        int status = 0;
        int r = process_wait_child(process, want, &status);
        if (r < 0) return SCRC(-ECHILD);
        if (r > 0) {
            if (wstatus) *(int*)wstatus = status;
            if (rusage) memset((void*)rusage, 0, RUSAGE_STRUCT_SIZE);
            return SCRC(r);
        }
        if (options & WNOHANG) return SCRC(0);

        scheduler_wait(&process->child_wq, seq);
    }
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
