#pragma once 

#include <types.h>
#include <constants.h>

typedef enum {
    KMT_LOG,
    KMT_ERROR,
    KMT_DEBUG,
#if UTOPIA_DEBUG == 1
    KMT_EXTRA_DEBUG
#endif
} kernel_message_type_t;

extern void printf(const char *fmt, ...);
extern void printk(const kernel_message_type_t type, const char *module, const char *fmt, ...);
extern void user_print(char* fmt, uint64_t size);
extern void printk_suspend_console();
extern void printk_remove_console_suspension();
extern void locked_printf(const char* fmt, ...);

#if UTOPIA_DEBUG == 1
#define dprintk(...) printk(KMT_EXTRA_DEBUG, __VA_ARGS__) 
#else
#define dprintk(...) \
    do {} while (0)
#endif
