#pragma once

#include <types.h>

int elf_start(const uint8_t* data, uintptr_t size, int syscall_conv, int arg);
