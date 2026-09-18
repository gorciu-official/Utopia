#include <types.h>

int strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char* strcpy(char* dest, const char* src) {
    char* orig = dest;
    while ((*dest++ = *src++) != '\0');
    return orig;
}

void str_reverse(char *str, int len) {
    int i = 0, j = len - 1;
    while (i < j) {
        char tmp = str[i];
        str[i] = str[j];
        str[j] = tmp;
        i++; j--;
    }
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    return (unsigned char)(*s1) - (unsigned char)(*s2);
}

int strncmp(const char *s1, const char *s2, uint64_t n) {
    if (n == 0)
        return 0;

    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }

    if (n == (uint64_t)-1)
        return 0;

    return (unsigned char)(*s1) - (unsigned char)(*s2);
}
