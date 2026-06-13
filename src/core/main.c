#include <string.h>
#include <types.h> 
#include <drivers/vga.h>
#include <drivers/screen.h>
#include <drivers/acpi.h>
#include <drivers/cpu.h>
#include <drivers/idt.h>
#include <drivers/memory.h>
#include <multiboot.h>
#include <drivers/framebuffer.h>
#include <drivers/internals/font-8x8.h>

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

void framebuffer_draw_char(
    uint32_t x,
    uint32_t y,
    char c,
    uint32_t fg,
    uint32_t bg
) {
    if ((unsigned char)c >= 128)
        c = '?';

    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = font8x8_basic[(uint8_t)c][row];

        for (uint32_t col = 0; col < 8; col++) {

            if (bits & (1 << col))
                framebuffer_putpixel(x + col, y + row, fg);
            else
                framebuffer_putpixel(x + col, y + row, bg);
        }
    }
}

void framebuffer_printstr(uint32_t x, uint32_t y, char* str, uint32_t fg, uint32_t bg) {
    for (int i = 0; i < strlen(str); i++) {
        framebuffer_draw_char(x + i * 8, y, str[i], fg, bg);
    }
}

void cpu_main() {
    while (true) continue;
}

void kmain(multiboot_info_t* mbd) {
    vga_clearscreen();
    printk("Core", "%s", UTOPIA_VERSION);
    
    gdt_init();
    pic_remap(0x20, 0x28);
    idt_init();

    memory_init(mbd);

    framebuffer_init(mbd);
    framebuffer_printstr(40, 50, "siema mordy", 0xFF0000, 0x000000);

    acpi_init();

    int cpu_count = acpi_count_cpus();
    if (cpu_count == 0) printk("Core", "Could not start APs: ACPI returned invalid number of CPUs: %d", cpu_count);
    else boot_all_aps(cpu_count);

    cpu_main();
}

extern uint8_t ap_alive_table[256];

void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;
    idt_init();
    cpu_main();
}
