#include <arch/x86_64/common.h>
#include <types.h>
#include <arch/x86_64/pmio.h>
#include <lib/screen.h>

bool used_before = false;

void timer_init(uint32_t frequency) {
    if (!used_before) {
        printk(
            "PIT", "Initializing PIT interrupts at frequency %u for CPU %d", 
            frequency, current_processor_id()
        );

        uint32_t divisor = 1193180 / frequency;

        arch_outb(0x43, 0x36);
        arch_outb(0x40, (uint8_t)(divisor & 0xFF));
        arch_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

        void tsc_init();
        tsc_init();
    }

    void lapic_init(void);
    lapic_init();
}
