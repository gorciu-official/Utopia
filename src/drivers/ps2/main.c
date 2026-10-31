#include <types.h>
#include <constants.h>

bool keyboard_should_putchar = false;

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

#include <drivers/arch-specific/ps2.h>
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

#define KBD_BUF_SIZE 256

static char            kbd_buf[KBD_BUF_SIZE];
static uint32_t        kbd_head = 0, kbd_tail = 0;
static spinlock_t      kbd_lock = { 0 };
static wait_queue_t    kbd_wq   = WAIT_QUEUE_INIT;

static void kbd_push(char c) {
    unsigned long flags = arch_save_interrupts();
    spinlock_acquire(&kbd_lock);
    uint32_t next = (kbd_head + 1) % KBD_BUF_SIZE;
    if (next != kbd_tail) { // full -> drop
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
    spinlock_release(&kbd_lock);
    arch_restore_interrupts(flags);

    scheduler_wake_all(&kbd_wq);
}

static bool kbd_pop(char* c) {
    unsigned long flags = arch_save_interrupts();
    spinlock_acquire(&kbd_lock);
    bool ok = kbd_head != kbd_tail;
    if (ok) {
        *c = kbd_buf[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    }
    spinlock_release(&kbd_lock);
    arch_restore_interrupts(flags);
    return ok;
}

int ps2_read(char* buffer, uint64_t size) {
    uint64_t n = 0;
    if (size == 0) return 0;

    for (;;) {
        uint64_t seq = wait_queue_snapshot(&kbd_wq); 

        char c;
        while (n < size && kbd_pop(&c)) {
            buffer[n++] = c;
            if (c == '\n') return n;
        }
        if (n == size) return n;

        scheduler_wait(&kbd_wq, seq);
    }
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
    } else if (key == 0x1C) {
        kbd_push('\n');
        return;
    }

    // backspace should be handled by userspace

    char character = (SHIFT_LEFT_PRESSED || SHIFT_RIGHT_PRESSED) ? shift_keymap[key] : keymap[key];

    if (character == 0) {
        return;
    }

    if (keyboard_should_putchar)
        framebuffer_putchar(character, 0xFFFFFF, 0x000000);

    kbd_push(character);
}

#endif
