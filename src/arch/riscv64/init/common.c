#include <types.h>
#include <arch/common.h>

#define UART_BASE 0x10000000UL
#define UART_THR 0 
#define UART_LSR 5  
#define UART_TX_READY (1 << 5)

uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

void arch_early_init() {

}

void arch_late_init() {

}

void arch_boot_aps() {

}

int arch_init_serial() {
    // nothing needs to be done
    return 0;
}

void arch_serial_putchar(char c) {
    volatile unsigned char *uart =
        (volatile unsigned char *)UART_BASE;

    while (!(uart[UART_LSR] & UART_TX_READY)) {
        // Wait until UART can accept another byte
    }

    uart[UART_THR] = (unsigned char)c;
}

void arch_ap_init() {

}
