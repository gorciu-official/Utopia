#pragma once

#include <arch/common.h>

void panic(const char* reason, registers_t* regs);
