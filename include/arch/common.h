#pragma once

#include <types.h>
#include <constants.h>

#define CPU_ARCH_MAX_CPUS 256

extern void arch_early_init(void);
extern void arch_general_init(void);
extern void arch_late_init(void);
extern void arch_ap_init(void);
extern void arch_boot_aps(void);

static inline uint32_t current_processor_id(void) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" 
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (1));
    return (ebx >> 24);
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    return 0;
#endif
}

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#include <arch/x86_64/registers.h>
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
#include <arch/riscv64/registers.h>
#endif

static inline uintptr_t read_pf_addr(void) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    uintptr_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    return 0;
#endif
}

static inline void arch_wfi() {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    __asm__ volatile("hlt" ::: "memory");
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
    __asm__ volatile("wfi" ::: "memory");
#endif
}

static inline void arch_cli() {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    __asm__ volatile("cli" ::: "memory");
#endif
}

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
#define arch_invi(int_no) \
    __asm__ volatile("int %0" :: "i"(int_no) : "memory")
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
// TODO: this is a bad idea but I don't have any better
#define arch_invi(int_no) \
    __asm__ volatile("wfi" ::: "memory")
#endif

// TODO: add irq lock for riscv64
static inline uint64_t arch_save_interrupts(void) {
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags) :: "memory");
    __asm__ volatile("cli" ::: "memory");
    return flags;
#endif
    return 0;
}

static inline void arch_restore_interrupts(uint64_t flags) {
    (void)flags;
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
    if (flags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
#endif
}

int arch_init_serial();
void arch_serial_putchar(char c);
