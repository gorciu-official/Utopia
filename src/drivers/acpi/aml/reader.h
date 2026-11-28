#pragma once

#include <types.h>
#include <mm/pmm.h>

#include "common.h"
#include "opcodes.h"

__attribute__((used))
static bool aml_read_byte(aml_parser_t* parser, uint8_t* byte) {
    if (parser->position >= parser->size)
        return false;

    *byte = parser->base[parser->position++];

    return true;
}

__attribute__((used))
static bool aml_read_bytes(
    aml_parser_t* parser,
    void* destination,
    size_t count
) {
    if (count > parser->size - parser->position)
        return false;

    memcpy(destination, parser->base + parser->position, count);
    parser->position += count;

    return true;
}

__attribute__((used))
static bool aml_peek_byte(aml_parser_t* parser, uint8_t* byte) {
    if (parser->position >= parser->size)
        return false;

    *byte = parser->base[parser->position];
    return true;
}

__attribute__((used))
static bool aml_read_u16(aml_parser_t* parser, uint16_t* value) {
    uint8_t bytes[2];

    if (!aml_read_bytes(parser, bytes, 2))
        return false;

    *value = bytes[0] | ((uint16_t)bytes[1] << 8);
    return true;
}

__attribute__((used))
static bool aml_read_u32(aml_parser_t* parser, uint32_t* value) {
    uint8_t bytes[4];

    if (!aml_read_bytes(parser, bytes, 4))
        return false;

    *value =
        ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);

    return true;
}

__attribute__((used))
static bool aml_read_u64(aml_parser_t* parser, uint64_t* value) {
    uint32_t lo, hi;

    if (!aml_read_u32(parser, &lo) || !aml_read_u32(parser, &hi))
        return false;

    *value = (uint64_t)lo | ((uint64_t)hi << 32);
    return true;
}

__attribute__((used))
static bool aml_read_pkg_length(aml_parser_t* parser, size_t* end) {
    size_t start = parser->position;
    uint8_t lead;

    if (!aml_read_byte(parser, &lead))
        return false;

    size_t extra = lead >> 6;
    size_t length = lead & (extra ? 0x0F : 0x3F);

    for (size_t i = 0; i < extra; i++) {
        uint8_t b;

        if (!aml_read_byte(parser, &b))
            return false;

        length |= (size_t)b << (4 + 8 * i);
    }

    if (length < parser->position - start)
        return false;

    if (length > parser->size - start)
        return false;

    *end = start + length;
    return true;
}

static bool aml_is_lead_name_char(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || c == '_';
}

static bool aml_is_name_char(uint8_t c) {
    return aml_is_lead_name_char(c) || (c >= '0' && c <= '9');
}

__attribute__((used))
static bool aml_read_name_seg(aml_parser_t* parser, char out[4]) {
    uint8_t seg[4];

    if (!aml_read_bytes(parser, seg, 4))
        return false;

    if (
        !aml_is_lead_name_char(seg[0]) ||
        !aml_is_name_char(seg[1]) ||
        !aml_is_name_char(seg[2]) ||
        !aml_is_name_char(seg[3])
    )
        return false;

    memcpy(out, seg, 4);
    return true;
}

__attribute__((used))
static bool aml_read_name_string(
    aml_parser_t* parser,
    char* out,
    size_t out_size
) {
    size_t len = 0;
    uint8_t b;

#define PUT(c) do { \
        if (len + 1 >= out_size) return false; \
        out[len++] = (c); \
    } while (0)

    if (!aml_peek_byte(parser, &b))
        return false;

    if (b == AML_ROOT_CHAR) {
        parser->position++;
        PUT('\\');
    } else {
        while (aml_peek_byte(parser, &b) && b == AML_PARENT_CHAR) {
            parser->position++;
            PUT('^');
        }
    }

    if (!aml_read_byte(parser, &b))
        return false;

    size_t segs;
    if (b == 0x00) {
        segs = 0;
    } else if (b == AML_DUAL_NAME_PFX) {
        segs = 2;
    } else if (b == AML_MULTI_NAME_PFX) {
        uint8_t n;

        if (!aml_read_byte(parser, &n))
            return false;

        segs = n;
    } else {
        parser->position--;
        segs = 1;
    }

    for (size_t i = 0; i < segs; i++) {
        char seg[4];

        if (!aml_read_name_seg(parser, seg))
            return false;

        if (i > 0)
            PUT('.');

        for (int j = 0; j < 4; j++)
            PUT(seg[j]);
    }

#undef PUT
    out[len] = '\0';
    return true;
}
