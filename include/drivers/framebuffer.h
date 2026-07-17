#pragma once

#include <types.h>
#include <constants.h>

#if BOOTLOADER == BOOTLOADER_CODE_GRUB
#include <boot/multiboot1.h>
extern void framebuffer_init(multiboot_info_t* mbd);
#endif

#if BOOTLOADER == BOOTLOADER_CODE_LIMINE
#include <boot/limine.h>
extern void framebuffer_init(struct limine_framebuffer* mbd);
#endif

extern void framebuffer_putchar(char c, uint32_t fg, uint32_t bg);
extern void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color);
extern void framebuffer_printstr(char* str, uint32_t fg, uint32_t bg);
extern uint32_t* framebuffer_get_addr();
extern uint32_t framebuffer_get_pitch();
extern uint32_t framebuffer_get_width();
extern uint32_t framebuffer_get_height();
extern void framebuffer_enable_backbuffer();
