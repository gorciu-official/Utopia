#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_RISCV64

#define UART_BASE 0x10000000UL
#define UART_THR 0 
#define UART_LSR 5  
#define UART_TX_READY (1 << 5)

int arch_init_serial() {
    // nothing needs to be done
    return 0;
}

void arch_serial_putchar(char c) {
    volatile unsigned char *uart =
        (volatile unsigned char *)UART_BASE;

    while (!(uart[UART_LSR] & UART_TX_READY)) {
        // wait until UART can accept another byte
    }

    uart[UART_THR] = (unsigned char)c;
}

#endif
