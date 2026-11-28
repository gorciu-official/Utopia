#include <constants.h>

#if ARCHITECTURE == ARCHITECTURE_CODE_x86_64

#include "definitions.h"
#include <lib/screen.h>
#include <mm/pmm.h>

#define ACPI_MADT_SIGNATURE 0x43495041
#define ACPI_HPET_SIGNATURE 0x54455048
#define ACPI_FADT_SIGNATURE 0x50434146
#define ACPI_DSDT_SIGNATURE "DSDT"
#define ACPI_RSDP_SIGNATURE "RSD PTR "

static acpi_madt_t* madt = NULL;
static acpi_rsdp_t* rsdp = NULL;
static acpi_hpet_t* hpet = NULL;
static acpi_fadt_t* fadt = NULL;
static acpi_sdt_header_t* dsdt = NULL;

static int acpi_checksum(void* table, size_t length) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)table;

    for (size_t i = 0; i < length; i++)
        sum += bytes[i];

    return sum == 0;
}

static acpi_rsdp_t* scan_region(uintptr_t start, uintptr_t end) {
    for (uintptr_t addr = start; addr < end; addr += 16) {
        acpi_rsdp_t* rsdp = (acpi_rsdp_t*)phys_to_virt(addr);

        if (!memcmp(rsdp->signature, ACPI_RSDP_SIGNATURE, 8)) {
            if (!acpi_checksum(rsdp, 20))
                continue;

            if (rsdp->revision >= 2 && rsdp->length >= 36) {
                if (!acpi_checksum(rsdp, rsdp->length))
                    continue;
            }

            printk(KMT_LOG, "ACPI", "Table RSDP found at %p", rsdp);

            return rsdp;
        }
    }

    return NULL;
}

acpi_rsdp_t* find_rsdp(void) {
    uint16_t* ebda_ptr = (uint16_t*)phys_to_virt(0x40E);
    uintptr_t ebda = ((uintptr_t)(*ebda_ptr)) << 4;

    if (ebda) {
        acpi_rsdp_t* r = scan_region(ebda, ebda + 1024);
        if (r) return r;
    }

    // TODO: there has to be a better way for this tho,
    //       it works in QEMU and in real hardware but
    //       is quite dirty
    printk(KMT_LOG, "ACPI", "Searching BIOS region");
    return scan_region(0xE0000, 0x100000);
}

static void* acpi_get_sdt_root(acpi_rsdp_t* rsdp) {
    if (!rsdp)
        return NULL;

    if (rsdp->revision >= 2 && rsdp->xsdt_address)
        return phys_to_virt(rsdp->xsdt_address);

    return phys_to_virt(rsdp->rsdt_address);
}

void* acpi_find_table(uint32_t signature) {
    void* root = acpi_get_sdt_root(rsdp);

    if (!root)
        return NULL;

    acpi_sdt_header_t* sdt = (acpi_sdt_header_t*)root;

    int entry_size = (rsdp->revision >= 2) ? 8 : 4;

    uint8_t* entries = (uint8_t*)sdt + sizeof(acpi_sdt_header_t);

    int count = (sdt->length - sizeof(acpi_sdt_header_t)) / entry_size;

    for (int i = 0; i < count; i++) {
        uint64_t addr = 0;

        if (entry_size == 8)
            addr = ((uint64_t*)entries)[i];
        else
            addr = ((uint32_t*)entries)[i];

        if (!addr)
            continue;

        acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)phys_to_virt(addr);

        if (*(uint32_t*)hdr->signature == signature) {
            if (acpi_checksum(hdr, hdr->length))
                return (acpi_hpet_t*)hdr;
        }
    }

    return NULL;
}

#define ACPI_DEFINE_FIND_TABLE_FUNCTION(table_name, signature, return_type) \
    return_type* acpi_find_##table_name() { \
        return (return_type*)acpi_find_table(signature); \
    }

ACPI_DEFINE_FIND_TABLE_FUNCTION(madt, ACPI_MADT_SIGNATURE, acpi_madt_t);
ACPI_DEFINE_FIND_TABLE_FUNCTION(hpet, ACPI_HPET_SIGNATURE, acpi_hpet_t);
ACPI_DEFINE_FIND_TABLE_FUNCTION(fadt, ACPI_FADT_SIGNATURE, acpi_fadt_t);

static uint64_t acpi_fadt_dsdt_address(acpi_fadt_t* fadt) {
    if (fadt->header.length >= sizeof(acpi_fadt_t) && fadt->x_dsdt)
        return fadt->x_dsdt;

    return fadt->dsdt;
}

static acpi_sdt_header_t* acpi_find_dsdt(acpi_fadt_t* fadt) {
    uint64_t address = acpi_fadt_dsdt_address(fadt);

    if (!address)
        return NULL;

    acpi_sdt_header_t* hdr = (acpi_sdt_header_t*)phys_to_virt(address);

    if (memcmp(hdr->signature, ACPI_DSDT_SIGNATURE, 4))
        return NULL;

    if (!acpi_checksum(hdr, hdr->length))
        return NULL;

    return hdr;
}

void* acpi_get_aml(size_t* length) {
    if (!dsdt)
        return NULL;

    if (length)
        *length = dsdt->length - sizeof(acpi_sdt_header_t);

    return (uint8_t*)dsdt + sizeof(acpi_sdt_header_t);
}

void acpi_init_aml(void) {
    fadt = acpi_find_fadt();
    if (!fadt) {
        printk(KMT_LOG, "ACPI", "No FADT found");
        return;
    }

    dsdt = acpi_find_dsdt(fadt);
    if (!dsdt) {
        printk(KMT_LOG, "ACPI", "No valid DSDT found");
        return;
    }

    size_t length = 0;
    void* aml = acpi_get_aml(&length);
    printk(KMT_LOG, "ACPI", "AML at %p, %d bytes; starting interpretter", aml, (int)length);
    acpi_aml_start_interpreter(aml, length);
}

int acpi_get_cpus(uint8_t* apic_ids, int max_cpus) {
    if (!madt)
        return 0;

    int count = 0;

    uint8_t* ptr = madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        acpi_madt_entry_header_t* e = (acpi_madt_entry_header_t*)ptr;

        if (e->type == 0) {
            acpi_madt_local_apic_t* cpu = (acpi_madt_local_apic_t*)ptr;

            if (cpu->flags & 1) {
                if (count < max_cpus) {
                    apic_ids[count] = cpu->apic_id;
                    count++;
                }
            }
        }

        ptr += e->length;
    }

    printk(KMT_LOG, "ACPI", "Found %d available CPUs.", count);

    return count;
}

void acpi_init() {
    printk(KMT_LOG, "ACPI", "Initializing ACPI");

    rsdp = find_rsdp();
    if (!rsdp) {
        printk(KMT_LOG, "ACPI", "No RSDP found");
        return;
    }

    madt = acpi_find_madt();
    if (!madt) {
        printk(KMT_LOG, "ACPI", "No MADT found");
        return;
    }

    hpet = acpi_find_hpet();
    if (!hpet) {
        printk(KMT_LOG, "ACPI", "No HPET found");
    }

    printk(KMT_LOG, "ACPI", "ACPI has been initialized successfully.");
}

#endif
