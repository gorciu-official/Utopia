#pragma once

#include <arch/x86_64/registers.h>

void panic(const char* reason, registers_t* regs);
