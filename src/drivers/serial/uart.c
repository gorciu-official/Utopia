#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_RISCV64

#define UART_BASE 0x10000000UL
#define UART_THR 0 
#define UART_LSR 5  
#define UART_TX_READY (1 << 5)

void arch_serial_putchar(char c) {
    volatile unsigned char *uart =
        (volatile unsigned char *)UART_BASE;

    while (!(uart[UART_LSR] & UART_TX_READY)) {
        // wait until UART can accept another byte
    }

    uart[UART_THR] = (unsigned char)c;
}

int arch_init_serial() {
    static const char clear_seq[] = "\x1b[2J\x1b[3J\x1b[H";
    for (unsigned i = 0; clear_seq[i] != '\0'; i++)
        arch_serial_putchar(clear_seq[i]);
    return 0;
}
#endif
