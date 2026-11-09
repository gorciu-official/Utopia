#include <types.h>
#include <arch/x86_64/common.h>

// this is 100% true and 0% opinionated
#define INTEL_MY_BELOVED ".intel_syntax noprefix\n"
#define FUCK_AT_AND_T    ".att_syntax\n"

static inline void enable_avx(void) {
    uint32_t eax, edx;

    asm volatile (
        INTEL_MY_BELOVED 
        "xor ecx, ecx\n"
        "xgetbv\n"
        FUCK_AT_AND_T
        : "=a"(eax), "=d"(edx)
        :
        : "ecx"
    );

    eax |= (1 << 1) | (1 << 2);

    asm volatile (
        INTEL_MY_BELOVED
        "xor ecx, ecx\n"
        "xsetbv\n"
        FUCK_AT_AND_T
        :
        : "a"(eax), "d"(edx)
        : "ecx"
    );
}

void enable_simd_extensions(void) {
    uint64_t rax, rbx, rcx, rdx; 

    // disables FPU emulation
    uint64_t cr0 = read_cr0();
    cr0 |= (1ULL << 1);
    cr0 |= (1ULL << 5);
    cr0 &= ~(1ULL << 2);
    cr0 &= ~(1ULL << 3);

    write_cr0(cr0);

    // enables SSE.
    // the AMD64 architecture specification demands that all x86_64 processors support
    // at least SSE and SSE2, so we do not need to check for that. osdev wiki claims that 
    // to support updates to SSE, no further configuration is needed
    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);

    // checks for AVX support and enables it
    __cpuid(1, rax, rbx, rcx, rdx);

    bool supports_xsave = rcx & (1ULL << 26);
    bool supports_avx = rcx & (1ULL << 28);

    if (supports_xsave && supports_avx) {
        cr4 |= (1ULL << 18); 
        
        write_cr4(cr4);

        enable_avx();
    } else 
        write_cr4(cr4);
}
