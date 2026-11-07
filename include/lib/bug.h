#pragma once

#include <drivers/framebuffer.h>
#include <lib/screen.h>
#include <panic.h>

#define BUG(name, ...)                                                           \
    do {                                                                         \
        printk_remove_console_suspension();                                      \
        printk(KMT_ERROR, "\033[1;91mBug:\033[0m" name, #__VA_ARGS__);           \
        panic("Kernel module reported bug occured", NULL);                       \
    } while (0)
