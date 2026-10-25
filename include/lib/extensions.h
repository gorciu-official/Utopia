#pragma once

#include <types.h>

#define likely_common(expr, expected)               \
    (                                               \
      __builtin_expect( !!(expr), (expected) )        \
    )

#define likely(expr) likely_common(expr, true)
#define unlikely(expr) likely_common(expr, false)
