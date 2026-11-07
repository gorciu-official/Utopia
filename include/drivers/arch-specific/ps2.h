#pragma once

#include <types.h>
#include <constants.h>
#include <config.h>

#if (ARCHITECTURE == ARCHITECTURE_CODE_x86_64) && defined(CONFIG_IO_PS2_KEYBOARD_SUPPORT)

extern void ps2_interrupt_handler();
extern int ps2_read(char* buffer, uint64_t size);

#endif
