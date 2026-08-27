#include <types.h> 
#include <lib/screen.h>
#include <drivers/acpi.h>
#include <arch/x86_64/common.h>
#include <drivers/idt.h>
#include <memory.h>
#include <drivers/framebuffer.h>
#include <drivers/filesystem.h>
#include <drivers/timer.h>
#include <drivers/pci.h>
#include <scheduler.h>
#include <process.h>
#include <panic.h>

#include <boot/common.h>

static inline void cpu_main() {
    while (true) continue;
}

void kmain(common_boot_structure_t* cbs) {
    framebuffer_init(
        cbs->framebuffer.addr,
        cbs->framebuffer.width, cbs->framebuffer.height,
        cbs->framebuffer.pitch, cbs->framebuffer.bpp
    );

    printk("Core", "Utopia %s", UTOPIA_VERSION);
    printk("Core", "  Source code: https://github.com/gorciu-official/Utopia");
    printk("Core", "  Licensed under GPL-v3.0");
    printk("Core", "---");

    if (cbs->modules.font_addr != NULL) {
        framebuffer_switch_font((psf1_header_t*)cbs->modules.font_addr, cbs->modules.font_size);
    }
    
    // cpu init
    enable_sse();
    gdt_init();
    pic_remap(0x20, 0x28);
    idt_init();
    timer_init(100);
    enable_umip();
    init_syscall();

    // misc init 
    #if BOOTLOADER == BOOTLOADER_CODE_GRUB
        memory_init_base(cbs->plain_mbd);
    #elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
        memory_init_base();
    #endif
    if (cbs->modules.initramfs_addr != NULL)
        memory_reserve_range(
            (uintptr_t)cbs->modules.initramfs_addr, 
            (uintptr_t)((char*)cbs->modules.initramfs_addr + cbs->modules.initramfs_size)
        );
    if (cbs->modules.font_addr != NULL)
        memory_reserve_range(
            (uintptr_t)cbs->modules.font_addr, 
            (uintptr_t)((char*)cbs->modules.font_addr + cbs->modules.font_size)
        );
    memory_init();
    framebuffer_enable_backbuffer();
    acpi_init();

    // scheduler init
    scheduler_init();
    process_init();

    // ap bootstrap
    uint8_t cpu_apic_id[CPU_ARCH_MAX_CPUS];
    int cpu_count = acpi_get_cpus(cpu_apic_id, CPU_ARCH_MAX_CPUS);

    if (cpu_count < 1) printk("Core", "Could not start APs: ACPI returned invalid number of CPUs: %d", cpu_count);
    else if (cpu_count == 1) printk("Core", "One CPU detected, skipping SMP initialization.");
    else boot_all_aps(cpu_apic_id, cpu_count);

    // init pci
    pci_scan_bus();

    // init filesystem
    vfs_init();
    vfs_register_driver(&ramfs_driver);
    vfs_register_driver(&tarfs_driver);
    if (cbs->modules.initramfs_addr != NULL) {
        tarfs_set_image(cbs->modules.initramfs_addr, cbs->modules.initramfs_size);
        vfs_mount("tarfs", 0, "/");
    } else 
        vfs_mount("ramfs", 0, "/");

    // run base tasks
    vnode_t* init_file = 0;
    vfs_lookup("/init", &init_file);
    if (init_file) {
        int elf_start(const uint8_t* elf, uintptr_t size);
        uint64_t size = init_file->size;
        void* buffer = malloc(size);
        uint64_t bytes_read = 0;
        init_file->ops->read(init_file, buffer, size, 0, &bytes_read);
        if (bytes_read == size) {
            int response = elf_start(buffer, size);
            if (response != 0)
                panic("INIT_LOAD_FAIL", NULL);
        }
    } else {
        panic("NO_INIT_FILE", NULL);
    }

    // suspend console output 
    #if UTOPIA_DEBUG == 0
        printk_suspend_console();
    #endif

    cpu_main();
}

extern uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;
    enable_sse();
    idt_init();
    gdt_init();
    timer_init(100);
    enable_umip();
    init_syscall();

    printk("Core", "CPU APIC ID %d fully ready, handing control to the scheduler.", id);
    
    scheduler_ap_init();
    cpu_main();
}
