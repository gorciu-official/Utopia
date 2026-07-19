#include <lib/string.h>
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
#include <constants.h>

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
#include <boot/multiboot1.h>
#endif

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
#include <boot/limine.h>
#endif

static inline void cpu_main() {
    while (true) continue;
}

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
void kmain(multiboot_info_t* mbd) {
    framebuffer_init(mbd);
#endif 
#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

void kmain() {
    struct limine_framebuffer* framebuffer = framebuffer_request.response->framebuffers[0];
    framebuffer_init(framebuffer);
#endif

    printk("Core", "%s", UTOPIA_VERSION);
    printk("Core", "  Source code: https://github.com/gorciu-official/Utopia");
    printk("Core", "  Licensed under GPL-v3.0");
    printk("Core", "---");

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
    char* cmdline = (char*) (uintptr_t) mbd->cmdline;
    bool cmdline_is_empty = strlen(cmdline) == 0;
    printk("Core", "Kernel command line: %s", cmdline_is_empty ? "<EMPTY>" : cmdline);
#endif
    
    // cpu init
    gdt_init();
    pic_remap(0x20, 0x28);
    idt_init();
    timer_init(100);
    enable_umip();
    init_syscall();

    // misc init 
#if BOOTLOADER == BOOTLOADER_CODE_GRUB
    memory_init(mbd);
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
    memory_init();
#endif
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
    
#ifdef WE_YALL_LOVE_C
    #define MAKE_THIS_AN_3D_ARRAY_BROTHER [26][256][20000000]
    #define DECLARE_VOLATILE_INTEGER volatile int
    #define DECLARE_THAT_VOID const void *
    #define DECLARE_THAT_SLOP(___s) DECLARE_VOLATILE_INTEGER (*(* const * const (* const ___s)MAKE_THIS_AN_3D_ARRAY_BROTHER)(DECLARE_THAT_VOID))[3]
    #define SEMAJKOLON ;
    #define ZNAK_ROWNOSCI_LEWICOWY_TAKI_FAJNY =
    #define WCALE_NIE_SLOP {0}
    #define KURWA(___s) DECLARE_THAT_SLOP(___s) ZNAK_ROWNOSCI_LEWICOWY_TAKI_FAJNY WCALE_NIE_SLOP SEMAJKOLON
    #define KURWA_MAC KURWA(JA_PIERDOLE) KURWA(KOCHAM_C)
    KURWA_MAC
#endif

    // init filesystem
    vfs_init();
    vfs_register_driver(&ramfs_driver);
    vfs_mount("ramfs", 0, "/");
    vnode_t *dir = 0;
    vnode_t *file = 0;
    vfs_lookup("/", &dir);
    dir->ops->mkdir(dir, "test_dir", &dir);
    dir->ops->create(dir, "hello.txt", &file);
    const char* msg = "this is a hello string! if u see this hello string that means u're a sigma";
    uint64_t written = 0;
    file->ops->write(file, msg, 17, 0, &written);

    // run base tasks
    int elf_start(const uint8_t* elf, uintptr_t size);
    static const unsigned char elf[] = { // minimal ELF from claude 
        0x7f,0x45,0x4c,0x46,0x02,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x02,0x00,0x3e,0x00,0x01,0x00,0x00,0x00,0x78,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
        0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x40,0x00,0x38,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,
        0x9f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x9f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0xbf,0x02,0x00,0x00,0x00,0x48,0x8d,0x35,
        0x18,0x00,0x00,0x00,0xba,0x03,0x00,0x00,0x00,0xb8,0x01,0x00,0x00,0x00,0x0f,0x05,
        0xbf,0x00,0x00,0x00,0x00,0xb8,0x3c,0x00,0x00,0x00,0x0f,0x05,0x74,0x61,0x6b
    };
    elf_start(elf, sizeof(elf));


    // suspend console output 
    printk_suspend_console();

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
