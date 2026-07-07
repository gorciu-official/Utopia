#pragma once 

#include <types.h>

extern void printf(const char *fmt, ...);
extern void printk(const char *module, const char *fmt, ...);
extern void user_print(char* fmt, uint64_t size);
extern void printk_suspend_console();
