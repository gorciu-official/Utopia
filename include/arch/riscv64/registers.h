#pragma once

#include <types.h>

typedef struct registers {
    uint64_t x[32];

    uint64_t scause;
    uint64_t stval;
    uint64_t sepc;
    uint64_t sstatus;
} __attribute__((packed)) registers_t;
