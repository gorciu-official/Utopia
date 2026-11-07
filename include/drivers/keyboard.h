#pragma once

#include <types.h>

typedef enum {
    // a keyboard key that can be represent using an ASCII character (like letters, numbers,
    // but also \b, \t and this kind of shit)
    KEYBOARD_PRESSED_KEY_KIND_CHAR
} keyboard_pressed_key_kind_t;

typedef struct {
    keyboard_pressed_key_kind_t kind;
    char character;
} keyboard_pressed_key_t;

extern void keyboard_handle_keypress(keyboard_pressed_key_t key);
extern int keyboard_read(char* buffer, uint64_t size);
