#include <drivers/framebuffer.h>
#include <drivers/screen.h>

static uint32_t* fb_addr = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

void framebuffer_init(multiboot_info_t* mbd) {
    if (!(mbd->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        printk("FB", "Framebuffer not available in Multiboot info");
        return;
    }

    if (mbd->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        printk("FB", "Framebuffer is not RGB mode (type: %d)", mbd->framebuffer_type);
        return;
    }

    fb_addr = (uint32_t*)(uintptr_t)mbd->framebuffer_addr;
    fb_width = mbd->framebuffer_width;
    fb_height = mbd->framebuffer_height;
    fb_pitch = mbd->framebuffer_pitch;
    fb_bpp = mbd->framebuffer_bpp;

    printk("FB", "Framebuffer initialized at %p (%dx%d, %d bpp, pitch %d)", 
           fb_addr, fb_width, fb_height, fb_bpp, fb_pitch);
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_addr || x >= fb_width || y >= fb_height) return;

    if (fb_bpp == 32) {
        uint32_t* pixel = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch + x * 4);
        *pixel = color;
    } else if (fb_bpp == 24) {
        uint8_t* pixel = (uint8_t*)fb_addr + y * fb_pitch + x * 3;
        pixel[0] = color & 0xFF;
        pixel[1] = (color >> 8) & 0xFF;
        pixel[2] = (color >> 16) & 0xFF;
    }
}

uint32_t* framebuffer_get_addr() { return fb_addr; }
uint32_t framebuffer_get_pitch() { return fb_pitch; }
uint32_t framebuffer_get_width() { return fb_width; }
uint32_t framebuffer_get_height() { return fb_height; }
