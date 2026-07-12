#include <drivers/cpu.h>
#include <drivers/screen.h>

#define CPUID_7_UMIP (1 << 9)

bool cpu_has_umip(void) {
    unsigned int a, b, c, d;

    __cpuid_count(7, 0, &a, &b, &c, &d);

    return (c & CPUID_7_UMIP);
}

// saw this in linux kernel logs and after some reading thought to implement this here;
// umip (user mode instruction prevention) is a feature that blocks ring 3 threads from
// accessing instructions like `sgdt` which allow to read internal cpu descriptor tables,
// enabling this blocks programs from fingerprinting certain parts of your kernel
void enable_umip(void) {
    if (!cpu_has_umip())
        return printk("Mitigations", "Skipping UIMP for CPU %d, feature unavailable", current_processor_id());


    unsigned long cr4 = read_cr4();
    cr4 |= CPU_CR4_UMIP;
    write_cr4(cr4);

    printk("Mitigations", "Enabled User Mode Instruction Prevention for CPU %d", current_processor_id());
}
