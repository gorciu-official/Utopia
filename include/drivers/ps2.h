#pragma once

#include <types.h>

extern void ps2_interrupt_handler();
extern int ps2_read(char* buffer, uint64_t size);
