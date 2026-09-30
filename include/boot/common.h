#pragma once

#include <types.h>

typedef struct {
    struct {
        uintptr_t font_size;
        char*     font_addr;
        uintptr_t initramfs_size;
        char*     initramfs_addr;
    } modules;
    struct {
        uintptr_t addr;
        uint8_t   bpp;
        uint32_t  width;
        uint32_t  height;
        uint32_t  pitch;
    } framebuffer;
    void* plain_mbd;
} common_boot_structure_t;
