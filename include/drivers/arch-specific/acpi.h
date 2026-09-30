#pragma once

#include <types.h>
#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

extern void acpi_init();
extern int acpi_get_cpus(uint8_t* apic_ids, int max_cpus);

#else 
#error This file should not be included outside x86_64 specific code
#endif
