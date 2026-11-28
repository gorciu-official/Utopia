#pragma once

#include <types.h>

#define AML_MAX_DEPTH       32
#define AML_NAME_MAX        256

typedef struct {
    const uint8_t* base;
    size_t size;
    size_t position;
} aml_parser_t;
