#include <types.h> 
#include <constants.h>

#include <lib/screen.h>
#include <lib/extensions.h>

#include <arch/common.h>
#include <boot/common.h>

#include <memory.h>

#include <drivers/framebuffer.h>
#include <drivers/filesystem.h>

#include <exec/scheduler.h>
#include <exec/process.h>
#include <exec/loaders/elf.h>

#include <panic.h>

void kmain(common_boot_structure_t* cbs) {
    framebuffer_init(
        cbs->framebuffer.addr,
        cbs->framebuffer.width, cbs->framebuffer.height,
        cbs->framebuffer.pitch, cbs->framebuffer.bpp
    );

    printk(KMT_LOG, "Core", "Utopia %s", UTOPIA_VERSION);
    printk(KMT_LOG, "Core", "    * Copyright: 2026 Gorciu and contributors");
    printk(KMT_LOG, "Core", "    * Source code: https://github.com/gorciu-official/Utopia");
    printk(KMT_LOG, "Core", "    * Licensed under GPL-v3.0");
    printk(KMT_LOG, "Core", "-----");

    printk(KMT_LOG, "Core", "Kernel command line: %s", cbs->cmdline ?: "(none)");

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

    arch_boot_aps();
    arch_late_init();

    // init filesystem
    vfs_init();
    vfs_register_driver(&ramfs_driver);
    vfs_register_driver(&tarfs_driver);
    vfs_register_driver(&devtmpfs_driver);
    if (cbs->modules.initramfs_addr != NULL) {
        tarfs_set_image(cbs->modules.initramfs_addr, cbs->modules.initramfs_size);
        vfs_mount("tarfs", 0, "/");
    } else 
        vfs_mount("ramfs", 0, "/");

    vnode_t* root_node;
    vnode_t* dev_node;
    vfs_lookup("/", &root_node);
    root_node->ops->mkdir(root_node, "dev", &dev_node);
    vfs_mount("devtmpfs", 0, "/dev/");

    // run base tasks
    printk(KMT_LOG, "Core", "Starting init process");

    vnode_t* init_file = 0;
    vfs_lookup("/init", &init_file);
    if likely (init_file) {
        uint64_t size = init_file->size;
        void* buffer = malloc(size);
        uint64_t bytes_read = 0;
        init_file->ops->read(init_file, buffer, size, 0, &bytes_read);
        if (bytes_read == size) {
            int response = elf_start(buffer, size, 0, 0);
            if (response != 0)
                panic("Failed to load init file", NULL);
        }
    } else {
        panic("No init file found", NULL);
    }

    // suspend console output 
    #if UTOPIA_DEBUG == 0
        printk_suspend_console();
    #endif

    for (;;)
        arch_wfi();
}

extern uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;

    arch_ap_init();

    scheduler_ap_init();

    for (;;)
        scheduler_reap();
}
