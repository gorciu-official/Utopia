#include <arch/common.h>
#include <arch/x86_64/common.h>
#include <drivers/timer.h>

void arch_early_init() {
    enable_sse();
    gdt_init();
    pic_remap(0x20, 0x28);
    idt_init();
    timer_init(100);
    enable_umip();
    init_syscall();
}

void arch_boot_aps(uint8_t* ids, int count) {
    return boot_all_aps(ids, count);
}

void arch_ap_init() {
    enable_sse();
    idt_init();
    gdt_init();
    timer_init(100);
    enable_umip();
    init_syscall();
}
