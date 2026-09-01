#include <arch/x86_64/registers.h>
#include <types.h>
#include <scheduler.h>
#include <lib/screen.h>
#include <memory.h>

extern uint64_t tsc_get_ns_time();
extern bool has_invariant_tsc();

#define LAPIC_BASE       0xFEE00000

#define LAPIC_ID         0x020
#define LAPIC_EOI        0x0B0
#define LAPIC_SVR        0x0F0

#define LAPIC_LVT_TIMER  0x320
#define LAPIC_TICR       0x380
#define LAPIC_TCCR       0x390
#define LAPIC_DIVIDE     0x3E0

#define TIMER_VECTOR     0x40

#define TIMER_HZ         1000
#define TIMER_DIVIDE     16

volatile uint32_t* lapic = (volatile uint32_t *)LAPIC_BASE;

static uint64_t lapic_ticks_per_second;

static inline volatile uint32_t* lapic_reg(uint32_t reg) {
    return (volatile uint32_t*)phys_to_virt(LAPIC_BASE + reg);
}

static inline void lapic_write(uint32_t reg, uint32_t value) {
    *lapic_reg(reg) = value;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return *lapic_reg(reg);
}

static uint64_t lapic_calibrate(void) {
    bool has_itsc = has_invariant_tsc();

    lapic_write(LAPIC_DIVIDE, 0x3); 
    lapic_write(LAPIC_LVT_TIMER, 0x10000 | TIMER_VECTOR);
    lapic_write(LAPIC_TICR, 0xFFFFFFFF);

    if (has_itsc) {
        uint64_t start = tsc_get_ns_time();
        while (tsc_get_ns_time() - start < 10000000)
            continue;
    } else {
        asm volatile("hlt");
        asm volatile("hlt");
    }

    uint32_t current = lapic_read(LAPIC_TCCR);
    uint32_t elapsed = 0xFFFFFFFF - current;

    if (!has_itsc)
        printk("Clock", "Warning: Used PIT to calibrate LAPIC timer, this is generally a bad idea");

    uint64_t ticks_per_second = (uint64_t)elapsed * 100;
    ticks_per_second *= TIMER_DIVIDE;
    return ticks_per_second;
}

registers_t* lapic_timer_handler(registers_t* regs) {
    lapic_write(LAPIC_EOI, 0);

    return scheduler_schedule(regs);
}

void lapic_init(void) {
    uint32_t svr = lapic_read(LAPIC_SVR);
    lapic_write(LAPIC_SVR, svr | (1 << 8));

    lapic_ticks_per_second = lapic_calibrate();

    lapic_write(LAPIC_DIVIDE, 0x3);

    lapic_write(
        LAPIC_LVT_TIMER,
        TIMER_VECTOR | (1 << 17)
    );

    // 1000 Hz = one interrupt every 1 ms.
    uint32_t initial_count = (uint32_t)(lapic_ticks_per_second / TIMER_HZ);

    lapic_write(LAPIC_TICR, initial_count);
}
