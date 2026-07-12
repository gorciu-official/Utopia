#include <spinlock.h>
#include <drivers/framebuffer.h>
#include <arguments.h>
#include <numbers.h>
#include <drivers/timer.h>
#include <drivers/cpu.h>

static uint32_t current_fg = 0xFFFFFF;
static uint32_t current_bg = 0x000000;
static uint32_t ansi_colors[] = {
    0x000000, // Black
    0xAA0000, // Red
    0x00AA00, // Green
    0xAA5500, // Brown
    0x0000AA, // Blue
    0xAA00AA, // Magenta
    0x00AAAA, // Cyan
    0xAAAAAA, // White
    0x555555, // Bright Black (Gray)
    0xFF5555, // Bright Red
    0x55FF55, // Bright Green
    0xFFFF55, // Bright Yellow
    0x5555FF, // Bright Blue
    0xFF55FF, // Bright Magenta
    0x55FFFF, // Bright Cyan
    0xFFFFFF  // Bright White
};

static spinlock_t fb_spinlock = {0};
static bool spinlock_initialized = false;

static void parse_ansi(const char* *str) {
    const char* s = *str;

    if (*s == '\x1b') s++;
    if (*s == '[') s++;

    while (*s && *s != 'm') {
        int val = 0;
        bool has_val = false;
        if (is_char_digit(*s)) {
            has_val = true;
            while (is_char_digit(*s)) {
                val = val * 10 + (*s - '0');
                s++;
            }
        }
        
        if (val == 0 && !has_val) {
            val = 0;
            has_val = true;
        }

        if (has_val) {
            if (val == 0) {
                current_fg = 0xFFFFFF;
                current_bg = 0x000000;
            } else if (val >= 30 && val <= 37) {
                current_fg = ansi_colors[val - 30];
            } else if (val >= 40 && val <= 47) {
                current_bg = ansi_colors[val - 40];
            } else if (val >= 90 && val <= 97) {
                current_fg = ansi_colors[val - 90 + 8];
            } else if (val >= 100 && val <= 107) {
                current_bg = ansi_colors[val - 100 + 8];
            } else if (val == 39) {
                current_fg = 0xFFFFFF;
            } else if (val == 49) {
                current_bg = 0x000000;
            } else if (val == 38 || val == 48) {
                if (*s == ';') {
                    s++;
                    int mode = 0;
                    while (is_char_digit(*s)) {
                        mode = mode * 10 + (*s - '0');
                        s++;
                    }
                    if (mode == 5) {
                        if (*s == ';') {
                            s++;
                            int color_idx = 0;
                            while (is_char_digit(*s)) {
                                color_idx = color_idx * 10 + (*s - '0');
                                s++;
                            }
                            if (color_idx < 16) {
                                if (val == 38) current_fg = ansi_colors[color_idx];
                                else current_bg = ansi_colors[color_idx];
                            }
                        }
                    } else if (mode == 2) {
                        uint32_t r = 0, g = 0, b = 0;
                        if (*s == ';') { s++; while (is_char_digit(*s)) { r = r * 10 + (*s - '0'); s++; } }
                        if (*s == ';') { s++; while (is_char_digit(*s)) { g = g * 10 + (*s - '0'); s++; } }
                        if (*s == ';') { s++; while (is_char_digit(*s)) { b = b * 10 + (*s - '0'); s++; } }
                        uint32_t color = (r << 16) | (g << 8) | b;
                        if (val == 38) current_fg = color;
                        else current_bg = color;
                    }
                }
            }
        }

        if (*s == ';') s++;
    }
    
    if (*s == 'm') s++;
    *str = s;
}

static void print_string_internal(const char* str) {
    while (*str) {
        if (str[0] == '\x1b' && str[1] == '[') {
            parse_ansi(&str);
        } else {
            framebuffer_putchar(*str, current_fg, current_bg);
            str++;
        }
    }
}

static void print_string_internal_limit(const char* str, uint64_t size) {
    const char* end = str + size;

    while (str < end) {
        if (str + 1 < end && str[0] == '\x1b' && str[1] == '[') {
            parse_ansi(&str);
        } else {
            framebuffer_putchar(*str, current_fg, current_bg);
            str++;
        }
    }
}

static void vprintf(const char* fmt, va_list args) {
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

void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

void user_print(char* fmt, uint64_t size) {
    spinlock_acquire(&fb_spinlock);
    print_string_internal_limit(fmt, size);
    spinlock_release(&fb_spinlock);
}

static bool console_suspended = false;

void printk(const char* module, const char *fmt, ...) {
    if (console_suspended) return;

    uint64_t flags = save_interrupts();

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

    printf("\x1b[37m[%s]\x1b[0m ", time_str); 
    printf("\x1b[36m%s\x1b[0m", module);
    print_string_internal(": ");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    framebuffer_putchar('\n', current_fg, current_bg);

    spinlock_release(&fb_spinlock);

    restore_interrupts(flags);
}

void printk_suspend_console() {
    printk("Framebuffer", "Suspending kernel console output");
    console_suspended = true;
}

void printk_remove_console_suspension() {
    console_suspended = false;
    printk("Framebuffer", "Ensuring console is not suspended");
}
