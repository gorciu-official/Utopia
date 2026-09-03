#include <types.h> 
#include <lib/screen.h>
#include <arch/common.h>
#include <memory.h>
#include <drivers/framebuffer.h>
#include <drivers/filesystem.h>
#include <drivers/pci.h>
#include <scheduler.h>
#include <process.h>
#include <panic.h>
#include <boot/common.h>
#include <constants.h>

static inline void cpu_main() {
    while (true)
#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64
        asm volatile("hlt");
#elif ARCHITECTURE == ARCHITECTURE_CODE_RISCV64
        asm volatile("wfi");
#endif
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
    arch_early_init();

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
    arch_general_init();

    // scheduler init
    scheduler_init();
    process_init();

    // ap bootstrap
    arch_boot_aps();

    // init pci
    arch_late_init();

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

    arch_ap_init();

    printk("Core", "CPU APIC ID %d fully ready, handing control to the scheduler.", id);
    
    scheduler_ap_init();
    cpu_main();
}
