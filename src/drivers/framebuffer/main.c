#include <boot/limine.h>
#include <drivers/framebuffer.h>
#include <lib/screen.h>
#include <memory.h>
#include <lib/string.h>
#include <arch/common.h>

#include "font-8x8.h"

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

#define PSF1_MODE512 0x01
#define PSF1_MODEHASTAB 0x02

#define FONT_WIDTH 8

static psf1_header_t* current_font = NULL;
static uintptr_t current_font_size = 0;

static uint32_t* fb_addr = NULL;
static uint32_t* backbuffer = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;

static bool serial_used = false;

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t framebuffer_font_height(void) {
    if (current_font != NULL)
        return current_font->charsize;

    return 8;
}

void framebuffer_switch_font(psf1_header_t* font, uintptr_t size) {
    if (font == NULL) {
        current_font = NULL;
        current_font_size = 0;

        if (cursor_y >= fb_height)
            cursor_y = 0;

        if (cursor_x >= fb_width)
            cursor_x = 0;

        return;
    }

    if (size < sizeof(psf1_header_t))
        return;

    if (font->magic[0] != PSF1_MAGIC0 || font->magic[1] != PSF1_MAGIC1)
        return;

    if (font->charsize == 0)
        return;

    uint32_t glyph_count = (font->mode & PSF1_MODE512) ? 512 : 256;
    uintptr_t glyph_data_size = (uintptr_t)glyph_count * font->charsize;
    uintptr_t required_size = sizeof(psf1_header_t) + glyph_data_size;

    if (size < required_size)
        return;

    current_font = font;
    current_font_size = size;

    if (cursor_x >= fb_width)
        cursor_x = 0;

    if (cursor_y >= fb_height)
        cursor_y = 0;
}

void framebuffer_init(
    uintptr_t addr,
    uint32_t width, uint32_t height,
    uint32_t pitch, uint8_t  bpp
) {
    if (!addr) 
        return printk("Framebuffer", "No framebuffer information provided by bootloader");

    fb_addr     = (uint32_t*)addr;
    fb_width    = width;
    fb_height   = height;
    fb_pitch    = pitch;
    fb_bpp      = bpp;
}

void framebuffer_flush(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!fb_addr || !backbuffer)
        return;

    if (x >= fb_width || y >= fb_height)
        return;

    if (x + width > fb_width)
        width = fb_width - x;

    if (y + height > fb_height)
        height = fb_height - y;

    if (fb_bpp == 32) {
        for (uint32_t row = 0; row < height; row++) {
            uint32_t* src = (uint32_t*)((uint8_t*)backbuffer + (y + row) * fb_pitch + x * 4);
            uint32_t* dest = (uint32_t*)((uint8_t*)fb_addr + (y + row) * fb_pitch + x * 4);

            size_t words = width;
            size_t words64 = words / 2;

            uint64_t* s64 = (uint64_t*)src;
            uint64_t* d64 = (uint64_t*)dest;

            for (size_t i = 0; i < words64; i++)
                d64[i] = s64[i];

            if (words % 2 != 0)
                dest[words - 1] = src[words - 1];
        }
    } else if (fb_bpp == 24) {
        for (uint32_t row = 0; row < height; row++) {
            uint8_t* src = (uint8_t*)backbuffer + (y + row) * fb_pitch + x * 3;
            uint8_t* dest = (uint8_t*)fb_addr + (y + row) * fb_pitch + x * 3;

            size_t bytes = width * 3;
            size_t bytes64 = bytes / 8;

            uint64_t* s64 = (uint64_t*)src;
            uint64_t* d64 = (uint64_t*)dest;

            for (size_t i = 0; i < bytes64; i++)
                d64[i] = s64[i];

            for (size_t i = bytes64 * 8; i < bytes; i++)
                dest[i] = src[i];
        }
    }
}

void framebuffer_enable_backbuffer() {
    if (!fb_addr)
        return;

    size_t size = fb_height * fb_pitch;
    backbuffer = malloc(size);

    if (backbuffer) {
        memcpy(backbuffer, fb_addr, size);
    } else {
        printk("Framebuffer", "Failed to allocate backbuffer, using direct access to VRAM.");
    }
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_addr || x >= fb_width || y >= fb_height)
        return;

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
    if (!fb_addr)
        return;

    uint32_t font_height = framebuffer_font_height();

    if (font_height > fb_height)
        font_height = fb_height;

    uint32_t bg = 0x000000;

    if (backbuffer) {
        uint8_t* dest = (uint8_t*)backbuffer;
        uint8_t* src = (uint8_t*)backbuffer + font_height * fb_pitch;
        size_t size = (fb_height - font_height) * fb_pitch;

        uint64_t* d64 = (uint64_t*)dest;
        uint64_t* s64 = (uint64_t*)src;
        size_t size64 = size / 8;

        for (size_t i = 0; i < size64; i++)
            d64[i] = s64[i];

        for (size_t i = size64 * 8; i < size; i++)
            dest[i] = src[i];

        if (fb_bpp == 32) {
            uint64_t bg64 = ((uint64_t)bg << 32) | bg;

            for (uint32_t y = fb_height - font_height; y < fb_height; y++) {
                uint32_t* row_ptr = (uint32_t*)((uint8_t*)backbuffer + y * fb_pitch);
                uint64_t* row_ptr64 = (uint64_t*)row_ptr;
                size_t width64 = fb_width / 2;

                for (size_t x = 0; x < width64; x++)
                    row_ptr64[x] = bg64;

                if (fb_width % 2 != 0)
                    row_ptr[fb_width - 1] = bg;
            }
        } else if (fb_bpp == 24) {
            for (uint32_t y = fb_height - font_height; y < fb_height; y++) {
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
        uint8_t* src = (uint8_t*)fb_addr + font_height * fb_pitch;
        size_t size = (fb_height - font_height) * fb_pitch;

        uint64_t* d64 = (uint64_t*)dest;
        uint64_t* s64 = (uint64_t*)src;
        size_t size64 = size / 8;

        for (size_t i = 0; i < size64; i++)
            d64[i] = s64[i];

        for (size_t i = size64 * 8; i < size; i++)
            dest[i] = src[i];

        if (fb_bpp == 32) {
            uint64_t bg64 = ((uint64_t)bg << 32) | bg;

            for (uint32_t y = fb_height - font_height; y < fb_height; y++) {
                uint32_t* row_ptr = (uint32_t*)((uint8_t*)fb_addr + y * fb_pitch);
                uint64_t* row_ptr64 = (uint64_t*)row_ptr;
                size_t width64 = fb_width / 2;

                for (size_t x = 0; x < width64; x++)
                    row_ptr64[x] = bg64;

                if (fb_width % 2 != 0)
                    row_ptr[fb_width - 1] = bg;
            }
        } else if (fb_bpp == 24) {
            for (uint32_t y = fb_height - font_height; y < fb_height; y++) {
                uint8_t* row_ptr = (uint8_t*)fb_addr + y * fb_pitch;

                for (uint32_t x = 0; x < fb_width; x++) {
                    row_ptr[x * 3] = bg & 0xFF;
                    row_ptr[x * 3 + 1] = (bg >> 8) & 0xFF;
                    row_ptr[x * 3 + 2] = (bg >> 16) & 0xFF;
                }
            }
        }
    }

    if (cursor_y >= font_height)
        cursor_y -= font_height;
    else
        cursor_y = 0;
}

void framebuffer_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg) {
    if (!fb_addr)
        return;

    uint8_t ch = (uint8_t)c;
    if (ch >= 128)
        ch = '?';

    uint32_t font_height = framebuffer_font_height();

    if (x + FONT_WIDTH > fb_width || y + font_height > fb_height)
        return;

    uint32_t* target = backbuffer ? backbuffer : fb_addr;

    uint8_t* glyph;
    bool lsb_first;

    if (current_font != NULL) {
        uint32_t glyph_count =
            (current_font->mode & PSF1_MODE512) ? 512 : 256;

        if (ch >= glyph_count)
            ch = '?';

        uint8_t* glyph_data =
            (uint8_t*)current_font + sizeof(psf1_header_t);

        glyph = glyph_data + (uintptr_t)ch * current_font->charsize;

        lsb_first = false;
    } else {
        glyph = (uint8_t*)font8x8_basic[ch];

        lsb_first = true;
    }

    for (uint32_t row = 0; row < font_height; row++) {
        uint8_t bits = glyph[row];

        for (uint32_t col = 0; col < FONT_WIDTH; col++) {
            uint8_t bit = lsb_first
                ? (bits >> col) & 1
                : (bits >> (7 - col)) & 1;

            uint32_t color = bit ? fg : bg;

            if (fb_bpp == 32) {
                uint32_t* pixel =
                    (uint32_t*)((uint8_t*)target +
                    (y + row) * fb_pitch +
                    (x + col) * 4);

                *pixel = color;
            } else if (fb_bpp == 24) {
                uint8_t* pixel =
                    (uint8_t*)target +
                    (y + row) * fb_pitch +
                    (x + col) * 3;

                pixel[0] = color & 0xFF;
                pixel[1] = (color >> 8) & 0xFF;
                pixel[2] = (color >> 16) & 0xFF;
            }
        }
    }

    if (backbuffer)
        framebuffer_flush(x, y, FONT_WIDTH, font_height);
}

void framebuffer_putchar(char c, uint32_t fg, uint32_t bg) {
    if (!serial_used)
        arch_init_serial();

    if (serial_used)
        arch_serial_putchar(c);

    uint32_t font_height = framebuffer_font_height();

    if (c == '\n') {
        cursor_x = 0;
        cursor_y += font_height;
    } else {
        framebuffer_draw_char(cursor_x, cursor_y, c, fg, bg);

        cursor_x += FONT_WIDTH;

        if (cursor_x + FONT_WIDTH > fb_width) {
            cursor_x = 0;
            cursor_y += font_height;
        }
    }

    if (cursor_y + font_height >= fb_height)
        framebuffer_scroll();
}

void framebuffer_printstr(char* str, uint32_t fg, uint32_t bg) {
    for (int i = 0; i < strlen(str); i++)
        framebuffer_putchar(str[i], fg, bg);
}

uint32_t* framebuffer_get_addr() {
    return fb_addr;
}

uint32_t framebuffer_get_pitch() {
    return fb_pitch;
}

uint32_t framebuffer_get_width() {
    return fb_width;
}

uint32_t framebuffer_get_height() {
    return fb_height;
}
