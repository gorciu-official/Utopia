#include <drivers/acpi.h>
#include <drivers/pci.h>
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

void arch_boot_aps() {
    uint8_t ids[CPU_ARCH_MAX_CPUS];
    uint32_t count = acpi_get_cpus(ids, CPU_ARCH_MAX_CPUS);
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

void arch_late_init() {
    pci_scan_bus();
}
