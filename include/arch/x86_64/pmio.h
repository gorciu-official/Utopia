#include <types.h>

uint8_t arch_inb(uint16_t port);
uint16_t arch_inw(uint16_t port);
void arch_outb(uint16_t port, uint8_t value);
void arch_outw(uint16_t port, uint16_t data);
uint32_t arch_inl(uint16_t port);
void arch_outl(uint16_t port, uint32_t val);