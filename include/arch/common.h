#pragma once

#include <types.h>
#include <constants.h>

#define CPU_ARCH_MAX_CPUS 256

extern void arch_early_init(void);
extern void arch_ap_init(void);
extern void arch_boot_aps(uint8_t* ids, int count);

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
