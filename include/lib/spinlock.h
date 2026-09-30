#pragma once

#include <types.h>

typedef struct {
    volatile int locked;
} spinlock_t;

static inline void spinlock_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        while (lock->locked) {
            __asm__ volatile("pause");
        }
    }
}

static inline void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}
