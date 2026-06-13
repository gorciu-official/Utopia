#include <drivers/framebuffer.h>
#include <drivers/screen.h>
#include <drivers/internals/font-8x8.h>
#include <drivers/internals/ports.h>
#include <string.h>

#define COM1 0x3F8

#define FONT_SCALE 1
#define FONT_WIDTH  (8 * FONT_SCALE)
#define FONT_HEIGHT (8 * FONT_SCALE)

static uint32_t* fb_addr = NULL;
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

void framebuffer_scroll() {
    if (!fb_addr) return;

    uint8_t* dest = (uint8_t*)fb_addr;
    uint8_t* src = (uint8_t*)fb_addr + FONT_HEIGHT * fb_pitch;
    size_t size = (fb_height - FONT_HEIGHT) * fb_pitch;

    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }

    uint32_t bg = 0x000000;

    for (uint32_t y = fb_height - FONT_HEIGHT;
         y < fb_height;
         y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            framebuffer_putpixel(x, y, bg);
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
    if ((unsigned char)c >= 128)
        c = '?';

    for (uint32_t row = 0; row < 8; row++) {
        uint8_t bits = font8x8_basic[(uint8_t)c][row];

        for (uint32_t col = 0; col < 8; col++) {

            uint32_t color =
                (bits & (1 << col))
                ? fg
                : bg;

            for (uint32_t sy = 0; sy < FONT_SCALE; sy++) {
                for (uint32_t sx = 0; sx < FONT_SCALE; sx++) {

                    framebuffer_putpixel(
                        x + col * FONT_SCALE + sx,
                        y + row * FONT_SCALE + sy,
                        color
                    );
                }
            }
        }
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
