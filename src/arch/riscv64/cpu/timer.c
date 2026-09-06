#include <types.h>
#include <arch/common.h>

static uint64_t timer_hz          = 10000000ULL;
static uint64_t timer_boot_val    = 0;
static bool     timer_initialized = false;

static inline uint64_t rdtime(void) {
    uint64_t t;
    asm volatile ("rdtime %0" : "=r"(t));
    return t;
}

uint64_t arch_get_ns_time(void) {
    if (!timer_initialized)
        return 0;
    return (rdtime() - timer_boot_val) * 1000000000ULL / timer_hz;
}

void timer_init() {
    timer_boot_val = rdtime();
    timer_initialized = true;
}
