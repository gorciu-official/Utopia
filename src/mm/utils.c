#include <types.h>

void* memcpy(void* dest, const void* src, size_t n) {
    uint64_t* d64 = (uint64_t*)dest;
    const uint64_t* s64 = (const uint64_t*)src;
    size_t n64 = n / 8;
    for (size_t i = 0; i < n64; i++) {
        d64[i] = s64[i];
    }
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = n64 * 8; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = a;
    const uint8_t* y = b;

    for (size_t i = 0; i < n; i++)
        if (x[i] != y[i]) return 1;

    return 0;
}

void* memset(void* dest, int val, size_t n) {
    uint64_t v64 = (uint8_t)val;
    v64 |= v64 << 8;
    v64 |= v64 << 16;
    v64 |= v64 << 32;

    uint64_t* d64 = (uint64_t*)dest;
    size_t n64 = n / 8;
    for (size_t i = 0; i < n64; i++) {
        d64[i] = v64;
    }
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = n64 * 8; i < n; i++) {
        d[i] = (uint8_t)val;
    }
    return dest;
}

void* memmove(void *dst, const void *src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;

    if (d < s) {
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        d += n;
        s += n;

        while (n--) {
            *--d = *--s;
        }
    }

    return dst;
}
