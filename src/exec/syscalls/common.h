#pragma once

#include <types.h>

typedef struct {
    uint64_t arg1, arg2, arg3, arg4, arg5, arg6, syscall_no;
} syscall_regs_t;

#define USER_HEAP_MAX 0x0000800000000000ULL

#define SYSCALL_DEFINE(platform, syscall_name) \
    static uintptr_t syscall_##platform##_##syscall_name(syscall_regs_t* regs, process_t* process, thread_t* thread) 

#define SYSCALL_DEFINE_LINUX(syscall_name) \
    SYSCALL_DEFINE(linux, syscall_name)
