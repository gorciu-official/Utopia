#include <types.h>
#include <constants.h>

#if (ARCHITECTURE == ARCHITECTURE_CODE_x86_64) && defined(CONFIG_IO_PS2_KEYBOARD_SUPPORT)

#include <drivers/arch-specific/ps2.h>
#include <drivers/keyboard.h>
#include <lib/screen.h>
#include <drivers/framebuffer.h>
#include <arch/x86_64/common.h>
#include <arch/x86_64/pmio.h>
#include <arch/common.h>
#include <exec/scheduler.h>
#include <lib/spinlock.h>

// keymaps copied from https://github.com/Interpuce/AurorOS/blob/main/src/drivers/keyboard/input.c 
// some code was adapted from there as well 

static char keymap[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
    'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static char shift_keymap[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z',
    'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static uint8_t SHIFT_LEFT_PRESSED = 0;
static uint8_t SHIFT_RIGHT_PRESSED = 0;

static uint8_t read_scancode() {
    if (!(arch_inb(0x64) & 0x01)) {
        return 0; 
    }
    return arch_inb(0x60);
}

void ps2_interrupt_handler() {
    uint8_t scancode = read_scancode();
    uint8_t SHIFT_RELEASED = scancode & 0x80;
    uint8_t key = scancode & ~0x80;

    // failsafe
    if (key == 0) {
        return;
    }

    if (SHIFT_RELEASED) {
        switch (key) {
        case 0x2A:
            SHIFT_LEFT_PRESSED = 0;
            break;
        case 0x36:
            SHIFT_RIGHT_PRESSED = 0;
            break;
        }
        return;
    } else if (key == 0x2A) {
        SHIFT_LEFT_PRESSED = 1;
        return;
    } else if (key == 0x36) {
        SHIFT_RIGHT_PRESSED = 1;
        return;
    }

    char character = (SHIFT_LEFT_PRESSED || SHIFT_RIGHT_PRESSED) ? shift_keymap[key] : keymap[key];
    keyboard_handle_keypress((keyboard_pressed_key_t){
        .kind = KEYBOARD_PRESSED_KEY_KIND_CHAR,
        .character = character
    });
}

#endif
