#include <exec/scheduler.h>
#include <exec/process.h>
#include <exec/loaders/elf.h>
#include <constants.h>
#include <arch/common.h>
#include <arch/memory.h>
#include <mm/pmm.h>
#include <lib/string.h>
#include <lib/screen.h>
#include <lib/extensions.h>
#include <fs/filesystem.h>

#include "common.h"

SYSCALL_DEFINE_OWN(exit) {
    (void)regs; (void)process; (void)thread;
    thread_exit();
    return SCRC(0);
}

SYSCALL_DEFINE_OWN(write) {
    (void)process; (void)thread;

    uintptr_t fd = regs->arg1;
    char* data = (char*)regs->arg2; 
    uintptr_t len = regs->arg3;

    if (fd == 1 || fd == 2) {
        user_print(data, len);
        return SCRC(len);
    }

    return SCRC(-1);
}

#define SYSCALL_OWN_ERR_INVALID_ARGUMENT   (-2)
#define SYSCALL_OWN_ERR_NO_FILE            (-3)
#define SYSCALL_OWN_ERR_MESSAGE_TOO_LARGE  (-7)
#define SYSCALL_OWN_ERR_BAD_EXEC           (-8)
#define SYSCALL_OWN_ERR_NO_MEMORY          (-9)

#define USER_VA_LIMIT       (1ULL << USER_VA_BITS)
#define EXEC_MAX_ARGS       1024
#define EXEC_MAX_PATH       4096
#define EXEC_MAX_ARG_BYTES  (256 * 1024)

static bool syscall_own_conv_valid(uint64_t conv) {
    if (conv == 0) return true;
#ifdef CONFIG_SYSCALL_ABI_LINUX
    if (conv == 1) return true;
#endif
    return false;
}

static void syscall_own_free_strvec(char** vec, int count) {
    if (!vec) return;
    for (int i = 0; i < count; i++) free(vec[i]);
    free(vec);
}

static int syscall_own_copy_strvec(char* const* user_vec, char*** out_vec, int* out_count, size_t* budget) {
    *out_vec = NULL;
    *out_count = 0;

    if (!user_vec) return 0;
    if ((uint64_t)(uintptr_t)user_vec >= USER_VA_LIMIT) return SYSCALL_OWN_ERR_INVALID_ARGUMENT;

    int count = 0;
    while (user_vec[count]) {
        if (++count > EXEC_MAX_ARGS) return SYSCALL_OWN_ERR_MESSAGE_TOO_LARGE;
        if ((uint64_t)(uintptr_t)&user_vec[count] >= USER_VA_LIMIT) return SYSCALL_OWN_ERR_INVALID_ARGUMENT;
    }

    char** vec = (char**)calloc((size_t)count + 1, sizeof(char*));
    if (!vec) return SYSCALL_OWN_ERR_NO_MEMORY;

    for (int i = 0; i < count; i++) {
        const char* src = user_vec[i];
        if ((uint64_t)(uintptr_t)src >= USER_VA_LIMIT) {
            syscall_own_free_strvec(vec, i);
            return SYSCALL_OWN_ERR_INVALID_ARGUMENT;
        }

        size_t len = (size_t)strlen(src) + 1;
        if (len > *budget) {
            syscall_own_free_strvec(vec, i);
            return SYSCALL_OWN_ERR_MESSAGE_TOO_LARGE;
        }
        *budget -= len;

        vec[i] = (char*)malloc(len);
        if (!vec[i]) {
            syscall_own_free_strvec(vec, i);
            return SYSCALL_OWN_ERR_NO_MEMORY;
        }
        memcpy(vec[i], src, len);
    }

    *out_vec = vec;
    *out_count = count;
    return 0;
}

typedef struct {
    char*    path;
    char**   argv;
    int      argc;
    char**   envp;
    int      envc;
    uint8_t* image;
    uint64_t size;
} own_exec_args_t;

static void own_exec_release(own_exec_args_t* a) {
    syscall_own_free_strvec(a->argv, a->argc);
    syscall_own_free_strvec(a->envp, a->envc);
    if (a->image) free(a->image);
    if (a->path) free(a->path);
    *a = (own_exec_args_t){0};
}

static int own_exec_load(syscall_regs_t* regs, own_exec_args_t* a) {
    *a = (own_exec_args_t){0};
    int err = 0;

    const char* user_path = (const char*)regs->arg1;
    if (!user_path || (uint64_t)(uintptr_t)user_path >= USER_VA_LIMIT)
        return SYSCALL_OWN_ERR_INVALID_ARGUMENT;

    size_t path_len = 0;
    while (user_path[path_len]) {
        if (++path_len >= EXEC_MAX_PATH) return SYSCALL_OWN_ERR_INVALID_ARGUMENT;
        if ((uint64_t)(uintptr_t)&user_path[path_len] >= USER_VA_LIMIT) return SYSCALL_OWN_ERR_INVALID_ARGUMENT;
    }
    path_len++;

    a->path = (char*)malloc(path_len);
    if (!a->path) return SYSCALL_OWN_ERR_NO_MEMORY;
    memcpy(a->path, user_path, path_len);

    size_t budget = EXEC_MAX_ARG_BYTES;
    err = syscall_own_copy_strvec((char* const*)regs->arg3, &a->argv, &a->argc, &budget);
    if (err) goto fail;
    err = syscall_own_copy_strvec((char* const*)regs->arg4, &a->envp, &a->envc, &budget);
    if (err) goto fail;

    if (a->argc == 0) {
        a->argv = (char**)calloc(2, sizeof(char*));
        if (!a->argv) { err = SYSCALL_OWN_ERR_NO_MEMORY; goto fail; }
        a->argv[0] = (char*)malloc(path_len);
        if (!a->argv[0]) { err = SYSCALL_OWN_ERR_NO_MEMORY; goto fail; }
        memcpy(a->argv[0], a->path, path_len);
        a->argc = 1;
    }

    vnode_t* node = NULL;
    vfs_lookup(a->path, &node);
    if (!node) { err = SYSCALL_OWN_ERR_NO_FILE; goto fail; }
    if (node->type == VNODE_TYPE_DIR || !node->ops || !node->ops->read || node->size < 4) {
        err = SYSCALL_OWN_ERR_BAD_EXEC;
        goto fail;
    }

    a->size = node->size;
    a->image = (uint8_t*)malloc(a->size);
    if (!a->image) { err = SYSCALL_OWN_ERR_NO_MEMORY; goto fail; }

    uint64_t total = 0;
    while (total < a->size) {
        uint64_t got = 0;
        if (node->ops->read(node, a->image + total, a->size - total, total, &got) != 0 || got == 0)
            break;
        total += got;
    }
    if (total != a->size) { err = SYSCALL_OWN_ERR_BAD_EXEC; goto fail; }

    return 0;

fail:
    own_exec_release(a);
    return err;
}

static create_elf_process_request_t own_exec_request(const own_exec_args_t* a, uint64_t conv) {
    return (create_elf_process_request_t){
        .syscall_conv = (int)conv,
        .arg = 0,
        .image = { .data = a->image, .size = a->size },
        .args = { .argv = a->argv, .argc = a->argc, .envp = a->envp }
    };
}

SYSCALL_DEFINE_OWN(spawn) {
    (void)thread; (void)process;

    uint64_t conv = regs->arg2;
    if (!syscall_own_conv_valid(conv))
        return SCRC(SYSCALL_OWN_ERR_INVALID_ARGUMENT);

    own_exec_args_t a;
    int err = own_exec_load(regs, &a);
    if (err) return SCRC(err);

    int response = elf_start(own_exec_request(&a, conv));
    own_exec_release(&a);

    return SCRC(response != 0 ? SYSCALL_OWN_ERR_BAD_EXEC : 0);
}

SYSCALL_DEFINE_OWN(become) {
    if (!process || !thread || !regs->frame)
        return SCRC(SYSCALL_OWN_ERR_INVALID_ARGUMENT);

    uint64_t conv = regs->arg2;
    if (!syscall_own_conv_valid(conv))
        return SCRC(SYSCALL_OWN_ERR_INVALID_ARGUMENT);

    own_exec_args_t a;
    int err = own_exec_load(regs, &a);
    if (err) return SCRC(err);

    create_elf_process_request_t req = own_exec_request(&a, conv);
    elf_prepared_t prepared;

    if (elf_prepare(&req, &prepared) != 0) {
        own_exec_release(&a);
        return SCRC(SYSCALL_OWN_ERR_BAD_EXEC);
    }

    const char* base = a.path;
    for (const char* c = a.path; *c; c++)
        if (*c == '/' && c[1]) base = c + 1;

    process->syscall_conv = (int)conv;
    process_replace(process, thread, regs->frame, &prepared, base);

    own_exec_release(&a);
    return SCRC(0);
}

static const syscall_fn_t syscall_own_table[] = {
    [5]    = syscall_own_write,
    [2000] = syscall_own_exit,
    [2001] = syscall_own_spawn,
    [2002] = syscall_own_become
};

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
static syscall_regs_t syscall_own_to_sregs(registers_t* regs) {
    return (syscall_regs_t) {
        .syscall_no = regs->rax,
        .arg1 = regs->rdx, .arg2 = regs->rdi, .arg3 = regs->rsi,
        .arg4 = regs->r10, .arg5 = regs->r9,  .arg6 = regs->r8
    };
}

static void syscall_own_set_return_val(int64_t val, registers_t* regs) {
    regs->rax = (uint64_t)val;
}
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
static syscall_regs_t syscall_own_to_sregs(registers_t* regs) {
    return (syscall_regs_t) {
        .syscall_no = regs->x[10], // a0

        .arg1 = regs->x[11], /* a1 */ .arg2 = regs->x[12], /* a2 */ .arg3 = regs->x[13], /* a3 */ 
        .arg4 = regs->x[14], /* a4 */ .arg5 = regs->x[15], /* a5 */ .arg6 = regs->x[17], /* a7 */
    };
}

static void syscall_own_set_return_val(int64_t val, registers_t* regs) {
    regs->x[10] = (uint64_t)val;
}
#endif

SYSCALL_ABI_DEFINE(own, syscall_own_table, syscall_own_to_sregs, syscall_own_set_return_val, -1)
