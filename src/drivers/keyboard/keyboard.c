#include <types.h>

#include <lib/extensions.h>
#include <lib/bug.h>
#include <lib/spinlock.h>

#include <exec/scheduler.h>

#include <drivers/keyboard.h>
#include <drivers/arch-specific/ps2.h>

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

bool keyboard_should_putchar = true;

void keyboard_handle_keypress(keyboard_pressed_key_t key) {
    if unlikely (key.kind != KEYBOARD_PRESSED_KEY_KIND_CHAR) 
        BUG("Unknown keypress type, expected %d, got %d", KEYBOARD_PRESSED_KEY_KIND_CHAR, key.kind);

    if unlikely (key.character == 0)
        return;

    if unlikely (key.character == '\r')
        key.character = '\n';

    if (keyboard_should_putchar) {
        if (key.character == '\b')
            framebuffer_delchar();
        else
            framebuffer_putchar(key.character, 0xFFFFFF, 0x000000);
    }

    kbd_push(key.character);
}

int keyboard_read(char* buffer, uint64_t size) {
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
