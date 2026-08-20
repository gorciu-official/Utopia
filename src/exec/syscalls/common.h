#pragma once

#include <arch/x86_64/registers.h>
#include <types.h>
#include <process.h>

typedef struct {
    uint64_t arg1, arg2, arg3, arg4, arg5, arg6, syscall_no;
} syscall_regs_t;

typedef uintptr_t (*syscall_fn_t)(syscall_regs_t* regs, process_t* process, thread_t* thread);

typedef struct {
    // syscall table
    const syscall_fn_t (*table)[];
    uint64_t table_size;
    
    // misc
    int64_t not_found_error;

    // registers
    syscall_regs_t (*to_sregs)(registers_t* source);
} syscall_abi_t;

#define USER_HEAP_MAX 0x0000800000000000ULL

#define SYSCALL_DEFINE(platform, syscall_name) \
    static uintptr_t syscall_##platform##_##syscall_name(syscall_regs_t* regs, process_t* process, thread_t* thread) 

#define SYSCALL_DEFINE_LINUX(syscall_name) \
    SYSCALL_DEFINE(linux, syscall_name)

#define SYSCALL_TABLE_SIZE(syscall_table) \
    (sizeof(syscall_table) / sizeof(syscall_table[0]))

#define SYSCALL_ABI_DECLARE(abi_name) \
    syscall_abi_t syscall_abi_##abi_name 

#define SYSCALL_ABI_DEFINE(abi_name, syscall_table, to_sregs_fun, not_found_err) \
    SYSCALL_ABI_DECLARE(abi_name) = \
        { .table = &syscall_table, .table_size = SYSCALL_TABLE_SIZE(syscall_table), .to_sregs = to_sregs_fun, .not_found_error = (not_found_err) };
