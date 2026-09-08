#include <types.h>
#include <arch/common.h>
#include <lib/screen.h>
#include <scheduler.h>
#include <arch/common.h>

#include "sbi.h"

#define NS_PER_SEC        1000000000ULL
#define TIMER_INTERVAL_NS 10000000ULL

static uint64_t timer_hz          = 10000000ULL;
static uint64_t timer_boot_val    = 0;
static bool     timer_initialized = false;

static inline uint64_t rdtime(void) {
    uint64_t t;
    asm volatile ("rdtime %0" : "=r"(t));
    return t;
}

static inline void enable_timer_source(void) {
    asm volatile ("csrs sie, %0" :: "r"(1ULL << 5));
}

static inline void enable_global_interrupts(void) {
    asm volatile ("csrs sstatus, %0" :: "r"(1ULL << 1));
}

static inline uint64_t ns_to_timer_ticks(uint64_t ns) {
    return ns * timer_hz / NS_PER_SEC;
}

uint64_t arch_get_ns_time(void) {
    if (!timer_initialized)
        return 0;

    return (rdtime() - timer_boot_val) * NS_PER_SEC / timer_hz;
}

void timer_schedule_next(registers_t** regs) {
    sbicall(
        SBI_EID_SET_TIMER, 0,
        rdtime() + ns_to_timer_ticks(TIMER_INTERVAL_NS)
    );
    if (regs != NULL)
        *regs = scheduler_schedule(*regs);
}

void timer_init(void) {
    timer_boot_val    = rdtime();
    timer_initialized = true;

    enable_timer_source();
    timer_schedule_next(NULL);
    enable_global_interrupts();
}
