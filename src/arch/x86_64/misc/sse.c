#include <types.h>
#include <arch/x86_64/common.h>

void enable_sse(void) {
    uint64_t cr0 = read_cr0();
    cr0 |= (1ULL << 1);
    cr0 &= ~(1ULL << 2);
    cr0 &= ~(1ULL << 3);

    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);
    cr4 |= (1ULL << 10);

    write_cr4(cr4);
}
