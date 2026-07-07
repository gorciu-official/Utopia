#include <boot/limine.h>
#include <drivers/framebuffer.h>
#include <drivers/screen.h>
#include <drivers/memory.h>
#include <drivers/internals/font-8x8.h>
#include <drivers/internals/ports.h>
#include <string.h>

#define COM1 0x3F8

#define FONT_SCALE 1
#define FONT_WIDTH  (8 * FONT_SCALE)
#define FONT_HEIGHT (8 * FONT_SCALE)

static uint32_t* fb_addr = NULL;
static uint32_t* backbuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

static bool serial_used = false;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static int init_serial() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);  
    outb(COM1 + 0, 0x03);   
    outb(COM1 + 1, 0x00); 
    outb(COM1 + 3, 0x03);  
    outb(COM1 + 2, 0xC7);  
    outb(COM1 + 4, 0x0B);  
    
    outb(COM1 + 4, 0x1E); 
    outb(COM1, 0xAE);
    if (inb(COM1) != 0xAE) return 1; 
    
    outb(COM1 + 4, 0x0F);

    serial_used = true;
    return 0;
}

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
void framebuffer_init(multiboot_info_t* mbd) {
    if (!(mbd->flags & MULTIBOOT_FLAG_FRAMEBUFFER)) {
        printk("Framebuffer", "Framebuffer not available in Multiboot info");
        return;
    }

    if (mbd->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        printk("Framebuffer", "Framebuffer is not RGB mode (type: %d)", mbd->framebuffer_type);
        return;
    }

    fb_addr = (uint32_t*)(uintptr_t)mbd->framebuffer_addr;
    fb_width = mbd->framebuffer_width;
    fb_height = mbd->framebuffer_height;
    fb_pitch = mbd->framebuffer_pitch;
    fb_bpp = mbd->framebuffer_bpp;
#elif BOOTLOADER == BOOTLOADER_CODE_LIMINE
void framebuffer_init(struct limine_framebuffer* framebuffer) {
    if (framebuffer == NULL) {
        printk("Framebuffer", "Framebuffer request not available");
        return;
    }

    fb_addr = (uint32_t*)framebuffer->address;
    fb_width = framebuffer->width;
    fb_height = framebuffer->height;
    fb_pitch = framebuffer->pitch;
    fb_bpp = framebuffer->bpp;
#endif
}

void framebuffer_flush(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!fb_addr || !backbuffer) return;

    if (x >= fb_width || y >= fb_height) return;
    if (x + width > fb_width) width = fb_width - x;
    if (y + height > fb_height) height = fb_height - y;

    if (fb_bpp == 32) {
        for (uint32_t row = 0; row < height; row++) {
            uint32_t* src = (uint32_t*)((uint8_t*)backbuffer + (y + row) * fb_pitch + x * 4);
            uint32_t* dest = (uint32_t*)((uint8_t*)fb_addr + (y + row) * fb_pitch + x * 4);
            
            size_t words = width;
            size_t words64 = words / 2;
            uint64_t* s64 = (uint64_t*)src;
            uint64_t* d64 = (uint64_t*)dest;
            for (size_t i = 0; i < words64; i++) {
                d64[i] = s64[i];
            }
            if (words % 2 != 0) {
                dest[words - 1] = src[words - 1];
            }
        }
    } else if (fb_bpp == 24) {
        for (uint32_t row = 0; row < height; row++) {
            uint8_t* src = (uint8_t*)backbuffer + (y + row) * fb_pitch + x * 3;
            uint8_t* dest = (uint8_t*)fb_addr + (y + row) * fb_pitch + x * 3;
            
            size_t bytes = width * 3;
            size_t bytes64 = bytes / 8;
            uint64_t* s64 = (uint64_t*)src;
            uint64_t* d64 = (uint64_t*)dest;
            for (size_t i = 0; i < bytes64; i++) {
                d64[i] = s64[i];
            }
            for (size_t i = bytes64 * 8; i < bytes; i++) {
                dest[i] = src[i];
            }
        }
    }
}

void framebuffer_enable_backbuffer() {
    if (!fb_addr) return;
    size_t size = fb_height * fb_pitch;
    backbuffer = malloc(size);
    if (backbuffer) {
        memcpy(backbuffer, fb_addr, size);
    } else {
        printk("Framebuffer", "Failed to allocate backbuffer, using direct access to VRAM.");
    }
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_addr || x >= fb_width || y >= fb_height) return;

    if (fb_bpp == 32) {
        if (backbuffer) {
            uint32_t* pixel_bb = (uint32_t*)((uint8_t*)backbuffer + y * fb_pitch + x * 4);
            *pixel_bb = color;
        }
        uint32_t* pixel_fb = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch + x * 4);
        *pixel_fb = color;
    } else if (fb_bpp == 24) {
        if (backbuffer) {
            uint8_t* pixel_bb = (uint8_t*)backbuffer + y * fb_pitch + x * 3;
            pixel_bb[0] = color & 0xFF;
            pixel_bb[1] = (color >> 8) & 0xFF;
            pixel_bb[2] = (color >> 16) & 0xFF;
        }
        uint8_t* pixel_fb = (uint8_t*)fb_addr + y * fb_pitch + x * 3;
        pixel_fb[0] = color & 0xFF;
        pixel_fb[1] = (color >> 8) & 0xFF;
        pixel_fb[2] = (color >> 16) & 0xFF;
    }
}

void framebuffer_scroll() {
    if (!fb_addr) return;

    uint32_t bg = 0x000000;

    if (backbuffer) {
        uint8_t* dest = (uint8_t*)backbuffer;
        uint8_t* src = (uint8_t*)backbuffer + FONT_HEIGHT * fb_pitch;
        size_t size = (fb_height - FONT_HEIGHT) * fb_pitch;

        uint64_t* d64 = (uint64_t*)dest;
        uint64_t* s64 = (uint64_t*)src;
        size_t size64 = size / 8;
        for (size_t i = 0; i < size64; i++) {
            d64[i] = s64[i];
        }
        for (size_t i = size64 * 8; i < size; i++) {
            dest[i] = src[i];
        }

        if (fb_bpp == 32) {
            uint64_t bg64 = ((uint64_t)bg << 32) | bg;
            for (uint32_t y = fb_height - FONT_HEIGHT; y < fb_height; y++) {
                uint32_t* row_ptr = (uint32_t*)((uint8_t*)backbuffer + y * fb_pitch);
                uint64_t* row_ptr64 = (uint64_t*)row_ptr;
                size_t width64 = fb_width / 2;
                for (size_t x = 0; x < width64; x++) {
                    row_ptr64[x] = bg64;
                }
                if (fb_width % 2 != 0) {
                    row_ptr[fb_width - 1] = bg;
                }
            }
        } else if (fb_bpp == 24) {
            for (uint32_t y = fb_height - FONT_HEIGHT; y < fb_height; y++) {
                uint8_t* row_ptr = (uint8_t*)backbuffer + y * fb_pitch;
                for (uint32_t x = 0; x < fb_width; x++) {
                    row_ptr[x * 3] = bg & 0xFF;
                    row_ptr[x * 3 + 1] = (bg >> 8) & 0xFF;
                    row_ptr[x * 3 + 2] = (bg >> 16) & 0xFF;
                }
            }
        }

        framebuffer_flush(0, 0, fb_width, fb_height);

    } else {
        uint8_t* dest = (uint8_t*)fb_addr;
        uint8_t* src = (uint8_t*)fb_addr + FONT_HEIGHT * fb_pitch;
        size_t size = (fb_height - FONT_HEIGHT) * fb_pitch;

        uint64_t* d64 = (uint64_t*)dest;
        uint64_t* s64 = (uint64_t*)src;
        size_t size64 = size / 8;
        for (size_t i = 0; i < size64; i++) {
            d64[i] = s64[i];
        }
        for (size_t i = size64 * 8; i < size; i++) {
            dest[i] = src[i];
        }

        if (fb_bpp == 32) {
            uint64_t bg64 = ((uint64_t)bg << 32) | bg;
            for (uint32_t y = fb_height - FONT_HEIGHT; y < fb_height; y++) {
                uint32_t* row_ptr = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch);
                uint64_t* row_ptr64 = (uint64_t*)row_ptr;
                size_t width64 = fb_width / 2;
                for (size_t x = 0; x < width64; x++) {
                    row_ptr64[x] = bg64;
                }
                if (fb_width % 2 != 0) {
                    row_ptr[fb_width - 1] = bg;
                }
            }
        } else if (fb_bpp == 24) {
            for (uint32_t y = fb_height - FONT_HEIGHT; y < fb_height; y++) {
                uint8_t* row_ptr = (uint8_t*)fb_addr + y * fb_pitch;
                for (uint32_t x = 0; x < fb_width; x++) {
                    row_ptr[x * 3] = bg & 0xFF;
                    row_ptr[x * 3 + 1] = (bg >> 8) & 0xFF;
                    row_ptr[x * 3 + 2] = (bg >> 16) & 0xFF;
                }
            }
        }
    }

    if (cursor_y >= FONT_HEIGHT)
        cursor_y -= FONT_HEIGHT;
    else
        cursor_y = 0;
}

void framebuffer_draw_char(
    uint32_t x,
    uint32_t y,
    char c,
    uint32_t fg,
    uint32_t bg
) {
    if (!fb_addr) return;
    if ((unsigned char)c >= 128)
        c = '?';

    if (x + 8 > fb_width || y + 8 > fb_height) return;

    uint32_t* target = backbuffer ? backbuffer : fb_addr;

    if (fb_bpp == 32) {
        for (uint32_t row = 0; row < 8; row++) {
            uint8_t bits = font8x8_basic[(uint8_t)c][row];
            uint32_t* row_ptr = (uint32_t*)((uint8_t*)target + (y + row) * fb_pitch + x * 4);
            row_ptr[0] = (bits & 1) ? fg : bg;
            row_ptr[1] = (bits & 2) ? fg : bg;
            row_ptr[2] = (bits & 4) ? fg : bg;
            row_ptr[3] = (bits & 8) ? fg : bg;
            row_ptr[4] = (bits & 16) ? fg : bg;
            row_ptr[5] = (bits & 32) ? fg : bg;
            row_ptr[6] = (bits & 64) ? fg : bg;
            row_ptr[7] = (bits & 128) ? fg : bg;
        }
    } else if (fb_bpp == 24) {
        for (uint32_t row = 0; row < 8; row++) {
            uint8_t bits = font8x8_basic[(uint8_t)c][row];
            uint8_t* row_ptr = (uint8_t*)target + (y + row) * fb_pitch + x * 3;
            for (uint32_t col = 0; col < 8; col++) {
                uint32_t color = (bits & (1 << col)) ? fg : bg;
                row_ptr[col * 3] = color & 0xFF;
                row_ptr[col * 3 + 1] = (color >> 8) & 0xFF;
                row_ptr[col * 3 + 2] = (color >> 16) & 0xFF;
            }
        }
    }

    if (backbuffer) {
        framebuffer_flush(x, y, 8, 8);
    }
}

void framebuffer_putchar(char c, uint32_t fg, uint32_t bg) {
    if (!serial_used)
        init_serial();

    if (serial_used) {
        while ((inb(COM1 + 5) & 0x20) == 0);
        outb(COM1, c);
    }

    if (c == '\n') {

        cursor_x = 0;
        cursor_y += FONT_HEIGHT;

    } else {

        framebuffer_draw_char(
            cursor_x,
            cursor_y,
            c,
            fg,
            bg
        );

        cursor_x += FONT_WIDTH;

        if (cursor_x + FONT_WIDTH > fb_width) {
            cursor_x = 0;
            cursor_y += FONT_HEIGHT;
        }
    }

    if (cursor_y + FONT_HEIGHT >= fb_height) {
        framebuffer_scroll();
    }
}

void framebuffer_printstr(char* str, uint32_t fg, uint32_t bg) {
    for (int i = 0; i < strlen(str); i++) {
        framebuffer_putchar(str[i], fg, bg);
    }
}

uint32_t* framebuffer_get_addr() { return fb_addr; }
uint32_t framebuffer_get_pitch() { return fb_pitch; }
uint32_t framebuffer_get_width() { return fb_width; }
uint32_t framebuffer_get_height() { return fb_height; }
