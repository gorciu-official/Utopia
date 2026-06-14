#include <types.h>
#include <drivers/internals/ports.h>

static volatile uint64_t tick = 0;

void timer_handler() {
    tick++;
}

void timer_init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint64_t timer_get_ticks() {
    return tick;
}
