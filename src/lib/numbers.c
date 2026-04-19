#include <string.h>

int is_char_digit(char c) {
    return c >= '0' && c <= '9';
}

int itoa(int value, char* str, int base) {
    int i = 0, neg = 0;
    if (value == 0) { str[i++] = '0'; str[i] = '\0'; return 1; }
    if (value < 0 && base == 10) { neg = 1; value = -value; }
    while (value) {
        int r = value % base;
        str[i++] = (r < 10) ? '0' + r : 'a' + (r - 10);
        value /= base;
    }
    if (neg) str[i++] = '-';
    str[i] = '\0';
    str_reverse(str, i);
    return i;
}
