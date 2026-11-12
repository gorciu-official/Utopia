#include <types.h>

#define PLIC_BASE       0x0c000000
#define UART_IRQ        10
#define PLIC_CONTEXT    1

#define PLIC_PRIORITY   (PLIC_BASE + UART_IRQ * 4)
#define PLIC_ENABLE     (PLIC_BASE + 0x2000 + PLIC_CONTEXT * 0x80)
#define PLIC_THRESHOLD  (PLIC_BASE + 0x200000 + PLIC_CONTEXT * 0x1000)
#define PLIC_CLAIM      (PLIC_THRESHOLD + 4)

void plic_init(void) {
    volatile uint32_t* priority = (volatile uint32_t*)PLIC_PRIORITY;
    volatile uint32_t* enable = (volatile uint32_t*)PLIC_ENABLE;
    volatile uint32_t* threshold = (volatile uint32_t*)PLIC_THRESHOLD;

    *priority = 1;
    *enable |= (1u << UART_IRQ);
    *threshold = 0;

    asm volatile("csrs sie, %0" :: "r"(1UL << 9) : "memory");
}

uint32_t plic_claim(void) {
    return *(volatile uint32_t*)PLIC_CLAIM;
}

void plic_complete(uint32_t irq) {
    *(volatile uint32_t*)PLIC_CLAIM = irq;
}
