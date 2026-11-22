#include <types.h>
#include <kernel/panic.h>

struct __attribute__((packed)) idtr {
    uint16_t limit;
    uint64_t base;
};

void arch_pm_do_reboot(void) {
    // this basically makes a triple fault guys

    struct idtr idtr = {
        .limit = 0,
        .base = 0
    };

    asm volatile (
        "lidt %0\n"
        "ud2"
        :
        : "m"(idtr)
    );

    panic("Unsuccessfull system reboot", NULL);
}
