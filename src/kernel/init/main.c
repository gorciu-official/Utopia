#include <types.h> 
#include <config.h>
#include <constants.h>

#include <lib/screen.h>
#include <lib/string.h>
#include <lib/extensions.h>

#include <arch/common.h>
#include <boot/common.h>

#include <mm/pmm.h>

#include <drivers/framebuffer.h>
#include <fs/filesystem.h>

#include <exec/scheduler.h>
#include <exec/process.h>
#include <exec/loaders/elf.h>

#include <kernel/panic.h>
#include <kernel/cmdline.h>

void kmain(common_boot_structure_t* cbs) {
    framebuffer_init(
        cbs->framebuffer.addr,
        cbs->framebuffer.width, cbs->framebuffer.height,
        cbs->framebuffer.pitch, cbs->framebuffer.bpp
    );

    printk(KMT_LOG, "Core", "Utopia %s (for %s %s, from commit %s)", UTOPIA_VERSION, BOOTLOADER_NAME, ARCHITECTURE_NAME, UTOPIA_COMMIT);
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
            hhdm_virt_to_phys(cbs->modules.initramfs_addr),
            hhdm_virt_to_phys(cbs->modules.initramfs_addr) + cbs->modules.initramfs_size
        );
    if (cbs->modules.font_addr != NULL)
        memory_reserve_range(
            hhdm_virt_to_phys(cbs->modules.font_addr),
            hhdm_virt_to_phys(cbs->modules.font_addr) + cbs->modules.font_size
        );
    memory_init();
    framebuffer_enable_backbuffer();
    arch_general_init();

    // cmdline 
    cmdline_t* cmdline = NULL;
    const char* custom_init_path = NULL;
    int init_abi_type = 0; // 0 means by default native ABI 
    int res = cmdline_parse(cbs->cmdline, &cmdline);
    if (res != 0 && res != CMDLINE_ERR_NULL_POINTER) {
        printk(KMT_ERROR, "Core", "Error %d while parsing command line!", res);
    } else {
        custom_init_path = cmdline_get(cmdline, "init");
        
        const char* init_abi_type_str = cmdline_get(cmdline, "init-abi");
        if (init_abi_type_str != NULL) {
            if (strcmp(init_abi_type_str, "linux") == 0) {
                init_abi_type = 1;
            } else {
                printk(KMT_ERROR, "Core", "While parsing command line: unknown init ABI type: %s", init_abi_type_str);
            }
        }
    }

    // scheduler init
    scheduler_init();
    process_init();

#ifdef CONFIG_SMP
    arch_boot_aps();
#else
    printk(KMT_LOG, "SMP", "SMP support not present in this kernel configuration.");
#endif 
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
    devtmpfs_register_devices();
    procfs_register_nodes(cbs->cmdline);

    // run base tasks
    printk(KMT_LOG, "Core", "Starting init process");

    vnode_t* init_file = 0;
    vfs_lookup(custom_init_path ?: "/init", &init_file);
    if likely (init_file) {
        uint64_t size = init_file->size;
        autofree void* buffer = malloc(size);
        uint64_t bytes_read = 0;
        init_file->ops->read(init_file, buffer, size, 0, &bytes_read);
        if (bytes_read == size) {
            // peak systemd debugging 
            static char* init_argv[] = { "/init", NULL };
            static char* init_envp[] = {
                "SYSTEMD_LOG_LEVEL=debug",
                "SYSTEMD_LOG_TARGET=console",
                NULL
            };

            int response = elf_start((create_elf_process_request_t){
                .image = {
                    .data = buffer,
                    .size = size
                },
                .args = { .envp = init_envp, .argc = 1, .argv = init_argv },
                .syscall_conv = init_abi_type
            });
            if (response != 0)
                panic("Failed to load init file", NULL);
        }
    } else {
        panic("No init file found", NULL);
    }

    // suspend console output 
#ifndef CONFIG_IO_PRINT_EXTRA_DEBUG_MESSAGES
        printk_suspend_console();
#endif

    for (;;)
        arch_wfi();
}

#ifdef CONFIG_SMP

extern uint8_t ap_alive_table[CPU_ARCH_MAX_CPUS];

__attribute__((used)) void ap_main() {
    uint32_t id = current_processor_id();
    ap_alive_table[id] = 1;

    arch_ap_init();

    scheduler_ap_init();

    for (;;)
        scheduler_reap();
}

#endif
