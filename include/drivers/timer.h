#pragma once
#include <types.h>

void timer_init(uint32_t frequency);
void timer_handler();
uint64_t timer_get_ticks();
