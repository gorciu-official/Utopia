#include <types.h> 
#include <drivers/vga.h>
#include <drivers/screen.h>
#include <drivers/acpi.h>

extern void idt_init();
extern void boot_all_aps(uint8_t total_cores);

#define UTOPIA_VERSION_MAJOR "1"
#define UTOPIA_VERSION_MINOR "0"
#define UTOPIA_VERSION_PATCH "0"
#define UTOPIA_BETA_STATE 1
#if UTOPIA_BETA_STATE == 1
    #define UTOPIA_VERSION \
        "Utopia beta@" \
        UTOPIA_VERSION_MAJOR "." UTOPIA_VERSION_MINOR "." UTOPIA_VERSION_PATCH
#else 
    #define UTOPIA_VERSION \
        "Utopia v" \
        UTOPIA_VERSION_MAJOR "." UTOPIA_VERSION_MINOR "." UTOPIA_VERSION_PATCH
#endif

void kmain() {
    vga_clearscreen();
    printk("Core", "%s", UTOPIA_VERSION);

    idt_init();
    acpi_init();

    int cpu_count = acpi_count_cpus();
    if (cpu_count == 0) printk("Core", "Could not start application processors: ACPI returned invalid number of CPUs: %d", cpu_count);
    else boot_all_aps(cpu_count);
}
