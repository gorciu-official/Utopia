#pragma once

#include <types.h>

#include <mm/pmm.h>

#define likely_common(expr, expected)               \
    (                                               \
      __builtin_expect( !!(expr), (expected) )        \
    )

#define likely(expr) likely_common(expr, true)
#define unlikely(expr) likely_common(expr, false)

// this is a planned keyword for C29, but we will implement it 
// quicker because it's good
#define countof(array) (sizeof(array) / sizeof(array[0]))

__attribute__((used))
static void free_cleanup(void* var) {
    free((void**)var);
}

#define autofree __attribute__((cleanup(free_cleanup)))
