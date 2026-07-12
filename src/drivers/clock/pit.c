#include <drivers/cpu.h>
#include <types.h>
#include <drivers/internals/ports.h>
#include <drivers/screen.h>

static volatile uint64_t tick = 0;
static volatile int32_t increment_tick_cpu = -1;

void timer_handler() {
    if (increment_tick_cpu != (volatile int32_t)current_processor_id()) return;
    tick++;
}

void timer_init(uint32_t frequency) {
    if (increment_tick_cpu == -1) increment_tick_cpu = current_processor_id();

    printk(
        "PIT", "Initializing PIT interrupts at frequency %u for CPU %d (%s)", 
        frequency, current_processor_id(), increment_tick_cpu == (int32_t)current_processor_id()
            ? "increments ticks"
            : "scheduler only"
    );

    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_get_ticks() {
    return tick;
}
