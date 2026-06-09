#include <types.h>

// memory utils
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
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

void* phys_to_virt(uint64_t phys) {
    return (void*)phys; // very tuff
}
