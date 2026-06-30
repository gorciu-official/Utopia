#pragma once 

#include <types.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_MAKE_ADDR(bus, slot, func, offset) \
    (uint32_t)( \
        0x80000000 | \
        ((uint32_t)bus << 16) | \
        ((uint32_t)slot << 11) | \
        ((uint32_t)func << 8) | \
        (offset & 0xFC) \
    )

typedef struct {
    uint16_t vendor, device;
    uint8_t class_code, subclass, prog_if, revision;
} pci_device_info_t;

extern void pci_scan_bus();
