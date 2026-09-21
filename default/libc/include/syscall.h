#pragma once

#include <stdint.h>

#define __LIBC_SYSCALL_REG0 "rax"
#define __LIBC_SYSCALL_ASM_REG0 "a"

#define __LIBC_SYSCALL_REG1 "rdi"
#define __LIBC_SYSCALL_ASM_REG1 "D"

#define __LIBC_SYSCALL_REG2 "rsi"
#define __LIBC_SYSCALL_ASM_REG2 "S"

#define __LIBC_SYSCALL_REG3 "rdx"
#define __LIBC_SYSCALL_ASM_REG3 "d"

#define __LIBC_SYSCALL_REG4 "r10"
#define __LIBC_SYSCALL_ASM_REG4 "r"

#define __LIBC_SYSCALL_REG5 "r8"
#define __LIBC_SYSCALL_ASM_REG5 "r"

#define __LIBC_SYSCALL_REG6 "r9"
#define __LIBC_SYSCALL_ASM_REG6 "r"

#define __LIBC_LOSS_REGISTERS "rcx", "r11"
#define __LIBC_SYSCALL_INSTRUCTION "syscall"

static inline uint64_t __libc_syscall0(uint64_t syscall_no) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "=" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall1(
    uint64_t syscall_no,
    uint64_t arg1
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall2(
    uint64_t syscall_no,
    uint64_t arg1,
    uint64_t arg2
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;
    register uint64_t a2  asm(__LIBC_SYSCALL_REG2) = arg2;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1),
          __LIBC_SYSCALL_ASM_REG2 (a2)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall3(
    uint64_t syscall_no,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;
    register uint64_t a2  asm(__LIBC_SYSCALL_REG2) = arg2;
    register uint64_t a3  asm(__LIBC_SYSCALL_REG3) = arg3;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1),
          __LIBC_SYSCALL_ASM_REG2 (a2),
          __LIBC_SYSCALL_ASM_REG3 (a3)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall4(
    uint64_t syscall_no,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;
    register uint64_t a2  asm(__LIBC_SYSCALL_REG2) = arg2;
    register uint64_t a3  asm(__LIBC_SYSCALL_REG3) = arg3;
    register uint64_t a4  asm(__LIBC_SYSCALL_REG4) = arg4;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1),
          __LIBC_SYSCALL_ASM_REG2 (a2),
          __LIBC_SYSCALL_ASM_REG3 (a3),
          __LIBC_SYSCALL_ASM_REG4 (a4)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall5(
    uint64_t syscall_no,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;
    register uint64_t a2  asm(__LIBC_SYSCALL_REG2) = arg2;
    register uint64_t a3  asm(__LIBC_SYSCALL_REG3) = arg3;
    register uint64_t a4  asm(__LIBC_SYSCALL_REG4) = arg4;
    register uint64_t a5  asm(__LIBC_SYSCALL_REG5) = arg5;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1),
          __LIBC_SYSCALL_ASM_REG2 (a2),
          __LIBC_SYSCALL_ASM_REG3 (a3),
          __LIBC_SYSCALL_ASM_REG4 (a4),
          __LIBC_SYSCALL_ASM_REG5 (a5)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}

static inline uint64_t __libc_syscall6(
    uint64_t syscall_no,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5,
    uint64_t arg6
) {
    register uint64_t ret asm(__LIBC_SYSCALL_REG0) = syscall_no;
    register uint64_t a1  asm(__LIBC_SYSCALL_REG1) = arg1;
    register uint64_t a2  asm(__LIBC_SYSCALL_REG2) = arg2;
    register uint64_t a3  asm(__LIBC_SYSCALL_REG3) = arg3;
    register uint64_t a4  asm(__LIBC_SYSCALL_REG4) = arg4;
    register uint64_t a5  asm(__LIBC_SYSCALL_REG5) = arg5;
    register uint64_t a6  asm(__LIBC_SYSCALL_REG6) = arg6;

    asm volatile (
        __LIBC_SYSCALL_INSTRUCTION
        : "+" __LIBC_SYSCALL_ASM_REG0 (ret)
        : __LIBC_SYSCALL_ASM_REG1 (a1),
          __LIBC_SYSCALL_ASM_REG2 (a2),
          __LIBC_SYSCALL_ASM_REG3 (a3),
          __LIBC_SYSCALL_ASM_REG4 (a4),
          __LIBC_SYSCALL_ASM_REG5 (a5),
          __LIBC_SYSCALL_ASM_REG6 (a6)
        : __LIBC_LOSS_REGISTERS, "memory"
    );

    return ret;
}
