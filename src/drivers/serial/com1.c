#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

#include <arch/x86_64/pmio.h>

#define COM1 0x3F8

int arch_init_serial() {
    arch_outb(COM1 + 1, 0x00);
    arch_outb(COM1 + 3, 0x80);
    arch_outb(COM1 + 0, 0x03);
    arch_outb(COM1 + 1, 0x00);
    arch_outb(COM1 + 3, 0x03);
    arch_outb(COM1 + 2, 0xC7);
    arch_outb(COM1 + 4, 0x0B);

    arch_outb(COM1 + 4, 0x1E);
    arch_outb(COM1, 0xAE);

    if (arch_inb(COM1) != 0xAE)
        return 1;

    arch_outb(COM1 + 4, 0x0F);

    return 0;
}

void arch_serial_putchar(char c) {
    while ((arch_inb(COM1 + 5) & 0x20) == 0);
    arch_outb(COM1, c);
}

#endif
