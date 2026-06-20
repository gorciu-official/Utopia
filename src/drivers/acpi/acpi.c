#include <types.h>
#include <drivers/screen.h>
#include <drivers/memory.h>

#define ACPI_MADT_SIGNATURE 0x43495041 
#define RSDP_SIGNATURE "RSD PTR "

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
} __attribute__((packed)) RSDP;

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
} __attribute__((packed)) ACPISDTHeader;

typedef struct {
    ACPISDTHeader header;
    uint64_t entries[];
} __attribute__((packed)) XSDT;

typedef struct {
    ACPISDTHeader header;
    uint32_t lapic_addr;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) MADT;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) MADTEntryHeader;

typedef struct {
    MADTEntryHeader header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) MADTLocalAPIC;

static MADT* madt = NULL;
static RSDP* rsdp = NULL;

static int acpi_checksum(void* table, size_t length) {
    uint8_t sum = 0;
    uint8_t* bytes = (uint8_t*)table;

    for (size_t i = 0; i < length; i++)
        sum += bytes[i];

    return sum == 0;
}
 
static RSDP* scan_region(uintptr_t start, uintptr_t end) {
    for (uintptr_t addr = start; addr < end; addr += 16) {
        RSDP* rsdp = (RSDP*)phys_to_virt(addr);

        if (!memcmp(rsdp->signature, RSDP_SIGNATURE, 8)) {
            if (!acpi_checksum(rsdp, 20))
                continue;

            if (rsdp->revision >= 2 && rsdp->length >= 36) {
                if (!acpi_checksum(rsdp, rsdp->length))
                    continue;
            }

            return rsdp;
        }
    }

    return NULL;
}

RSDP* find_rsdp(void) {
    uint16_t* ebda_ptr = (uint16_t*)phys_to_virt(0x40E);
    uintptr_t ebda = ((uintptr_t)(*ebda_ptr)) << 4;

    if (ebda) {
        RSDP* r = scan_region(ebda, ebda + 1024);
        if (r) return r;
    }

    printk("ACPI", "Searching BIOS region...");

    return scan_region(0xE0000, 0x100000);
}

static void* acpi_get_sdt_root(RSDP* rsdp) {
    if (!rsdp)
        return NULL;

    if (rsdp->revision >= 2 && rsdp->xsdt_address)
        return phys_to_virt(rsdp->xsdt_address);

    return phys_to_virt(rsdp->rsdt_address);
}

MADT* acpi_find_madt() {
    void* root = acpi_get_sdt_root(rsdp);

    if (!root)
        return NULL;

    ACPISDTHeader* sdt = (ACPISDTHeader*)root;

    int entry_size = (rsdp->revision >= 2) ? 8 : 4;

    uint8_t* entries = (uint8_t*)sdt + sizeof(ACPISDTHeader);

    int count = (sdt->length - sizeof(ACPISDTHeader)) / entry_size;

    for (int i = 0; i < count; i++) {
        uint64_t addr = 0;

        if (entry_size == 8)
            addr = ((uint64_t*)entries)[i];
        else
            addr = ((uint32_t*)entries)[i];

        if (!addr)
            continue;

        ACPISDTHeader* hdr = (ACPISDTHeader*)phys_to_virt(addr);

        if (*(uint32_t*)hdr->signature == ACPI_MADT_SIGNATURE) {
            if (acpi_checksum(hdr, hdr->length))
                return (MADT*)hdr;
        }
    }

    return NULL;
}

int acpi_count_cpus() {
    if (!madt)
        return 0;

    int cpu_count = 0;

    uint8_t* ptr = madt->entries;
    uint8_t* end = (uint8_t*)madt + madt->header.length;

    while (ptr < end) {
        MADTEntryHeader* e = (MADTEntryHeader*)ptr;

        if (e->type == 0) {
            MADTLocalAPIC* cpu = (MADTLocalAPIC*)ptr;

            if (cpu->flags & 1)
                cpu_count++;
        }

        ptr += e->length;
    }

    printk("ACPI", "Found %d available CPUs.", cpu_count);

    return cpu_count;
}

void acpi_init() {
    printk("ACPI", "Initializing ACPI...");

    rsdp = find_rsdp();
    if (!rsdp) {
        printk("ACPI", "No RSDP found");
        return;
    }

    madt = acpi_find_madt();
    if (!madt) {
        printk("ACPI", "No MADT found");
        return;
    }

    printk("ACPI", "ACPI has been initialized successfully.");
}
