#include <string.h>
#include <types.h> 
#include <drivers/screen.h>
#include <drivers/acpi.h>
#include <drivers/cpu.h>
#include <drivers/idt.h>
#include <drivers/memory.h>
#include <drivers/framebuffer.h>
#include <limine.h>

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

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

void cpu_main() {
    while (true) continue;
}

static void hcf() {
    for (;;) {
        asm ("hlt");
    }
}

void kmain() {
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    struct limine_framebuffer* framebuffer = framebuffer_request.response->framebuffers[0];

    framebuffer_init(framebuffer);

    printk("Core", "%s", UTOPIA_VERSION);

    framebuffer_putchar('U', 0xFFFFFF, 0x000000);
    
    /*
    // cpu init
    gdt_init();
    pic_remap(0x20, 0x28);
    idt_init();

    // misc init 
    memory_init(mbd);
    acpi_init();

    // ap bootstrap
    int cpu_count = acpi_count_cpus();
    if (cpu_count == 0) printk("Core", "Could not start APs: ACPI returned invalid number of CPUs: %d", cpu_count);
    else boot_all_aps(cpu_count);

    cpu_main();
    */
    
    hcf();
}

extern uint8_t ap_alive_table[256];

void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;
    idt_init();
    cpu_main();
}
