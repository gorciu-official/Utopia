#pragma once

#include <types.h>

#include <memory.h>

#define likely_common(expr, expected)               \
    (                                               \
      __builtin_expect( !!(expr), (expected) )        \
    )

#define likely(expr) likely_common(expr, true)
#define unlikely(expr) likely_common(expr, false)

__attribute__((used))
static void free_cleanup(void* var) {
    free((void**)var);
}

#define autofree __attribute__((cleanup(free_cleanup)))
