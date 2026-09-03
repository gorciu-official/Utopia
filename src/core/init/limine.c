#include <types.h>
#include <constants.h>
#include <memory.h>
#include <lib/string.h>
#include <boot/common.h>
#include <boot/limine.h>

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE

void kmain(common_boot_structure_t* cbs);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

void kinit() {
    struct limine_framebuffer_response* framebuffer_res = framebuffer_request.response;
    common_boot_structure_t             cbs             = {0};
    struct limine_framebuffer*          framebuffer     = framebuffer_res ? framebuffer_res->framebuffers[0] : NULL;

    if (framebuffer) {
        // as you may have guessed by the look of these 
        // declarations, i am a perfectionist
        cbs.framebuffer.addr   = (uintptr_t)framebuffer->address;
        cbs.framebuffer.width  =            framebuffer->width;
        cbs.framebuffer.height =            framebuffer->height;
        cbs.framebuffer.pitch  =            framebuffer->pitch;
        cbs.framebuffer.bpp    =            framebuffer->bpp;
    } 

    if (module_request.response) {
        uint64_t module_count = module_request.response->module_count;
        struct limine_file** modules = module_request.response->modules;
    
        for (uint64_t i = 0; i < module_count; i++) {
            struct limine_file* mod = modules[i];
            if (strcmp(mod->path, "/initramfs.tar") == 0) {
                cbs.modules.initramfs_addr = mod->address;
                cbs.modules.initramfs_size = mod->size;
            } else if (strcmp(mod->path, "/font.psf1") == 0) {
                cbs.modules.font_addr = mod->address;
                cbs.modules.font_size = mod->size;
            }
        }
    }

    kmain(&cbs);
}

#endif
