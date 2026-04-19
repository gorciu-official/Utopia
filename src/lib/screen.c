#include <drivers/vga.h>
#include <arguments.h>
#include <numbers.h>

#define VGA_COLOR(fg, bg) (uint8_t)((bg << 4) | (fg))

static int current_fg = 7;
static int current_bg = 0; 

static uint8_t ansi_to_vga(int code) {
    if (code >= 30 && code <= 37) return code - 30;
    if (code >= 40 && code <= 47) return code - 40;
    if (code >= 90 && code <= 97) return (code - 90) + 8;
    if (code >= 100 && code <= 107) return (code - 100) + 8;
    return 0;
}

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
            current_fg = 7;
            current_bg = 0;
        } else if (val >= 30 && val <= 37) {
            current_fg = ansi_to_vga(val);
        } else if (val >= 40 && val <= 47) {
            current_bg = ansi_to_vga(val);
        } else if (val >= 90 && val <= 97) {
            current_fg = ansi_to_vga(val);
        } else if (val >= 100 && val <= 107) {
            current_bg = ansi_to_vga(val);
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
            vga_printchar(*str, VGA_COLOR(current_fg, current_bg));
            str++;
        }
    }
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[64];

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    char *s = va_arg(args, char*);
                    print_string_internal(s ? s : "(null)");
                    break;
                }
                case 'd': {
                    itoa(va_arg(args, int), buffer, 10);
                    print_string_internal(buffer);
                    break;
                }
                case 'x': {
                    itoa(va_arg(args, int), buffer, 16);
                    print_string_internal(buffer);
                    break;
                }
                case 'c': {
                    vga_printchar((char)va_arg(args, int), VGA_COLOR(current_fg, current_bg));
                    break;
                }
                case '%': vga_printchar('%', VGA_COLOR(current_fg, current_bg)); break;
            }
        } else if (*fmt == '\x1b') {
            parse_ansi(&fmt);
            continue; 
        } else {
            vga_printchar(*fmt, VGA_COLOR(current_fg, current_bg));
        }
        fmt++;
    }
    va_end(args);
}
