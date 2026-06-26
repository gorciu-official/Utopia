#include <string.h>
#include <types.h> 
#include <drivers/screen.h>
#include <drivers/acpi.h>
#include <drivers/cpu.h>
#include <drivers/idt.h>
#include <drivers/memory.h>
#include <multiboot.h>
#include <drivers/framebuffer.h>
#include <drivers/timer.h>
#include <scheduler.h>

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

static inline void cpu_main() {
    while (true) continue;
}

static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;

    __asm__ volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

extern void syscall_entry();

void init_syscall() {
    uint64_t efer = read_msr(0xC0000080);
    efer |= 1; 
    write_msr(0xC0000080, efer);
    
    uint64_t star =
        ((uint64_t)0x08 << 32) |
        ((uint64_t)(0x20 - 16) << 48);
    
    write_msr(0xC0000081, star);write_msr(0xC0000082, (uint64_t)syscall_entry);write_msr(0xC0000084, (1 << 9)); // IF = off during syscall
}

void kmain(multiboot_info_t* mbd) {
    framebuffer_init(mbd);
    printk("Core", "%s", UTOPIA_VERSION);
    printk("Core", "  Source code: https://github.com/gorciu-official/Utopia");
    printk("Core", "  Licensed under GPL-v3.0");
    printk("Core", "---");

    char* cmdline = (char*) (uintptr_t) mbd->cmdline;
    bool cmdline_is_empty = strlen(cmdline) == 0;

    printk("Core", "Kernel command line: %s", cmdline_is_empty ? "<EMPTY>" : cmdline);
    
    // cpu init
    pic_remap(0x20, 0x28);
    idt_init();
    gdt_init();
    timer_init(100);
    enable_umip();
    init_syscall();

    // misc init 
    memory_init(mbd);
    framebuffer_enable_backbuffer();
    acpi_init();

    // scheduler init
    scheduler_init();

    // ap bootstrap
    int cpu_count = acpi_count_cpus();
    if (cpu_count < 1) printk("Core", "Could not start APs: ACPI returned invalid number of CPUs: %d", cpu_count);
    else if (cpu_count == 1) printk("Core", "One CPU detected, skipping SMP initialization.");
    else boot_all_aps(cpu_count);

    // run base tasks
    extern char ring_3_program_end[];
    extern char ring_3_program[];
    const uintptr_t ring_3_program_size = (uintptr_t)ring_3_program_end - (uintptr_t)ring_3_program;
    void* ring_3_allocated_mem = malloc(ring_3_program_size);
    memcpy(ring_3_allocated_mem, ring_3_program, ring_3_program_size);
    set_page_permissions((uintptr_t)ring_3_allocated_mem, PAGE_PRESENT | PAGE_RW | PAGE_USER);
    set_page_executable((uintptr_t)ring_3_allocated_mem, true);
    thread_create("task3", ring_3_allocated_mem, NULL, 3);

    cpu_main();
}

extern uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;
    idt_init();
    gdt_init();
    timer_init(100);
    enable_umip();
    init_syscall();

    printk("Core", "CPU APIC ID %d fully ready, handing control to the scheduler.", id);
    
    scheduler_ap_init();
    cpu_main();
}
