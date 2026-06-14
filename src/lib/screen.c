#include <spinlock.h>
#include <drivers/framebuffer.h>
#include <arguments.h>
#include <numbers.h>
#include <drivers/timer.h>

static uint32_t current_fg = 0xFFFFFF;
static uint32_t current_bg = 0x000000;
static spinlock_t fb_spinlock = {0};
static bool spinlock_initialized = false;

static void parse_ansi(const char **str) {
    const char *s = *str;

    if (*s == '\x1b') s++;
    if (*s == '[') s++;

    while (*s && *s != 'm') {
        int val = 0;
        if (is_char_digit(*s)) {
            while (is_char_digit(*s)) {
                val = val * 10 + (*s - '0');
                s++;
            }
        }
        
        if (val == 0) {
            current_fg = 0xFFFFFF;
            current_bg = 0x000000;
        }

        if (*s == ';') s++;
    }
    
    if (*s == 'm') s++;
    *str = s;
}

static void print_string_internal(const char *str) {
    while (*str) {
        if (str[0] == '\x1b' && str[1] == '[') {
            parse_ansi(&str);
        } else {
            framebuffer_putchar(*str, current_fg, current_bg);
            str++;
        }
    }
}

static void vprintf(const char *fmt, va_list args) {
    char buffer[64]; 

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            bool is_long = false;
            if (*fmt == 'l') {
                is_long = true;
                fmt++;
            }

            switch (*fmt) {
                case 's': {
                    char *s = va_arg(args, char*);
                    print_string_internal(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    if (is_long) ltoa(va_arg(args, int64_t), buffer, 10);
                    else ltoa(va_arg(args, int32_t), buffer, 10);
                    print_string_internal(buffer);
                    break;
                }
                case 'u': {
                    if (is_long) ultoa(va_arg(args, uint64_t), buffer, 10);
                    else ultoa(va_arg(args, uint32_t), buffer, 10);
                    print_string_internal(buffer);
                    break;
                }
                case 'x': {
                    if (is_long) ultoa(va_arg(args, uint64_t), buffer, 16);
                    else ultoa(va_arg(args, uint32_t), buffer, 16);
                    print_string_internal(buffer);
                    break;
                }
                case 'p': {
                    print_string_internal("0x");
                    ultoa(va_arg(args, uint64_t), buffer, 16);
                    print_string_internal(buffer);
                    break;
                }
                case 'c': {
                    framebuffer_putchar((char)va_arg(args, int), current_fg, current_bg);
                    break;
                }
                case '%': framebuffer_putchar('%', current_fg, current_bg); break;
            }
        } else if (*fmt == '\x1b') {
            parse_ansi(&fmt);
            continue; 
        } else {
            framebuffer_putchar(*fmt, current_fg, current_bg);
        }
        fmt++;
    }
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void printk(const char *module, const char *fmt, ...) {
    // that should be pretty safe because
    // the first log comes from bootstrap processor
    // and there is no need to use locks anyways 
    if (!spinlock_initialized) {
        spinlock_init(&fb_spinlock);
        spinlock_initialized = true;
    }
    spinlock_acquire(&fb_spinlock);

    uint64_t ticks = timer_get_ticks();
    uint64_t seconds = ticks / 100;
    uint64_t milliseconds = (ticks % 100) * 10;
    char time_str[32];
    
    ultoa(seconds, time_str, 10);
    int len = 0;
    while(time_str[len]) len++;
    time_str[len++] = '.';
    
    char ms_str[16];
    ultoa(milliseconds, ms_str, 10);
    
    if (milliseconds < 10) {
        time_str[len++] = '0';                
        time_str[len++] = '0';
    } else if (milliseconds < 100) {
        time_str[len++] = '0';
    }
    
    for(int i = 0; ms_str[i]; i++) {
        time_str[len++] = ms_str[i];
    }
    time_str[len] = '\0';

    print_string_internal("[");
    print_string_internal(time_str);
    print_string_internal("] ");

    print_string_internal(module);
    print_string_internal(": ");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    framebuffer_putchar('\n', current_fg, current_bg);

    spinlock_release(&fb_spinlock);
}
