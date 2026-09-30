#pragma once

#include <types.h>
#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

extern void ps2_interrupt_handler();
extern int ps2_read(char* buffer, uint64_t size);

#else 
#error This file should not be included outside x86_64 specific code
#endif
