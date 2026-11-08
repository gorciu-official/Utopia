#pragma once

#include <drivers/framebuffer.h>
#include <lib/screen.h>
#include <kernel/panic.h>

#define BUG(name, ...)                                                                 \
    do {                                                                               \
        printk_remove_console_suspension();                                            \
        printk(KMT_ERROR, "Core", "\033[1;93mBug\033[0m: " name, ##__VA_ARGS__);       \
        panic("A bug was reported by a kernel module", NULL);                          \
    } while (0)
