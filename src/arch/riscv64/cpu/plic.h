#include <types.h>

extern void plic_init(void);
extern uint32_t plic_claim(void);
extern void plic_complete(uint32_t irq);

#define IS_PLIC_INTERRUPT(intno) \
    intno == 9
