#pragma once

#include <types.h>
#include <multiboot.h>

void framebuffer_init(multiboot_info_t* mbd);
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t* framebuffer_get_addr();
uint32_t framebuffer_get_pitch();
uint32_t framebuffer_get_width();
uint32_t framebuffer_get_height();
