#pragma once

#include <types.h>
#include <constants.h>

extern void framebuffer_init(
    uintptr_t addr,
    uint32_t width, uint32_t height,
    uint32_t pitch, uint8_t  bpp
);

typedef struct {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} __attribute__((packed)) psf1_header_t;

extern void      framebuffer_putchar(char c, uint32_t fg, uint32_t bg);
extern void      framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color);
extern void      framebuffer_printstr(char* str, uint32_t fg, uint32_t bg);
extern uint32_t* framebuffer_get_addr();
extern uint32_t  framebuffer_get_pitch();
extern uint32_t  framebuffer_get_width();
extern uint32_t  framebuffer_get_height();
extern void      framebuffer_enable_backbuffer();
extern void      framebuffer_switch_font(psf1_header_t* font, uintptr_t size);
