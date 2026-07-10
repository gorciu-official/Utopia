#pragma once

#include <types.h>

extern void acpi_init();
extern int acpi_get_cpus(uint8_t* apic_ids, int max_cpus);
