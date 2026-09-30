#pragma once

#include <types.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oemid[6];
    uint8_t revision;
    uint32_t rsdt_address;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t entries[];
} __attribute__((packed)) XSDT;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_header_t;

typedef struct {
    acpi_madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_local_apic_t;

typedef struct {
    acpi_sdt_header_t header;
    union {
        uint32_t raw_value;
        struct {
            uint8_t  hardware_revision_id; 
            uint8_t  number_of_comparators;
            uint8_t  counter_size_cap   : 1;     
            uint8_t  reserved           : 1;     
            uint8_t  legacy_irq_cap     : 1;     
            uint16_t pci_vendor_id;
        };
    } event_timer_block_id;
} __attribute__((packed)) acpi_hpet_t;
