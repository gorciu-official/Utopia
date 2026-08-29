#include <types.h>
#include <constants.h>
#include <memory.h>
#include <lib/string.h>
#include <boot/common.h>
#include <boot/multiboot1.h>

#if BOOTLOADER == BOOTLOADER_CODE_GRUB

void kmain(common_boot_structure_t* cbs);

void kinit(multiboot_info_t* mbd) {
    common_boot_structure_t cbs = {0};

    // TODO: this is temporary. it should be removed after memory map porting
    cbs.plain_mbd = mbd;

    if (
        mbd->flags & MULTIBOOT_FLAG_FRAMEBUFFER 
        && mbd->framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB
    ) {
        cbs.framebuffer.addr = (uintptr_t)phys_to_virt(mbd->framebuffer_addr);
        cbs.framebuffer.width = mbd->framebuffer_width;
        cbs.framebuffer.height = mbd->framebuffer_height;
        cbs.framebuffer.pitch = mbd->framebuffer_pitch;
        cbs.framebuffer.bpp = mbd->framebuffer_bpp;
    }

    if (mbd->mods_count > 0) {
        uint32_t count = mbd->mods_count;
        multiboot_module_t* mods = phys_to_virt((uintptr_t)mbd->mods_addr);

        for (unsigned int i = 0; i < count; i++) {
            multiboot_module_t mod = mods[i];

            if (strcmp(phys_to_virt((uintptr_t)mod.cmdline), "initramfs.tar") == 0) {
                cbs.modules.initramfs_addr = phys_to_virt((uintptr_t)mod.mod_start);
                cbs.modules.initramfs_size = mod.mod_end - mod.mod_start;
            } else if (strcmp(phys_to_virt((uintptr_t)mod.cmdline), "font.psf1") == 0) {
                cbs.modules.font_addr = phys_to_virt((uintptr_t)mod.mod_start);
                cbs.modules.font_size = mod.mod_end - mod.mod_start;
            }
        }
    }

    kmain(&cbs);
}

#endif 
