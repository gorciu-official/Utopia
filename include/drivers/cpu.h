#pragma once

#include <types.h>

#define CPU_ARCH_MAX_CPUS 256

#define __cpuid(level, a, b, c, d) \
    __asm__ volatile ("cpuid" \
        : "=a" (a), "=b" (b), "=c" (c), "=d" (d) \
        : "0" (level))

// copied from somewhere lmfao
static inline void __cpuid_count(unsigned int level, unsigned int count,
                                 unsigned int *a, unsigned int *b,
                                 unsigned int *c, unsigned int *d)
{
    __asm__ volatile ("cpuid"
        : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d)
        : "a" (level), "c" (count));
}

static inline uint32_t current_processor_id(void) {
    uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return (ebx >> 24);
}

extern void boot_all_aps(uint8_t total_cores);
extern void gdt_init();
extern void enable_umip(void);
extern void init_syscall();

static inline uint64_t save_interrupts(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags) :: "memory");
    __asm__ volatile("cli" ::: "memory");
    return rflags;
}

static inline void restore_interrupts(uint64_t rflags) {
    if (rflags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

#define CPU_CR4_UMIP (1UL << 11)

static inline uintptr_t read_cr2(void) {
    uintptr_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}
static inline unsigned long read_cr4(void) {
    unsigned long val;
    asm volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(unsigned long val) {
    asm volatile ("mov %0, %%cr4" :: "r"(val) : "memory");
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(val) : "memory");
}

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;

    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low  = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t high = (uint32_t)(value >> 32);

    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
        : "memory"
    );
}
