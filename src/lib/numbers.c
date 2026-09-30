#include <lib/string.h>
#include <lib/numbers.h>
#include <types.h>

int is_char_digit(char c) {
    return c >= '0' && c <= '9';
}

int ultoa(uint64_t value, char* str, int base) {
    int i = 0;
    if (value == 0) { str[i++] = '0'; str[i] = '\0'; return 1; }
    while (value) {
        uint64_t r = value % base;
        str[i++] = (r < 10) ? '0' + r : 'a' + (r - 10);
        value /= base;
    }
    str[i] = '\0';
    str_reverse(str, i);
    return i;
}

int ltoa(int64_t value, char* str, int base) {
    int i = 0, neg = 0;
    if (value == 0) { str[i++] = '0'; str[i] = '\0'; return 1; }
    if (value < 0 && base == 10) { neg = 1; value = -value; }
    
    // For bases other than 10, we treat value as unsigned
    if (base != 10) {
        return ultoa((uint64_t)value, str, base);
    }

    while (value) {
        int64_t r = value % base;
        if (r < 0) r = -r;
        str[i++] = (r < 10) ? '0' + r : 'a' + (r - 10);
        value /= base;
    }
    if (neg) str[i++] = '-';
    str[i] = '\0';
    str_reverse(str, i);
    return i;
}

int itoa(int value, char* str, int base) {
    return ltoa((int64_t)value, str, base);
}
