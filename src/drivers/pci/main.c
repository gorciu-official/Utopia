#include <types.h>
#include <arch/x86_64/io.h>
#include <lib/screen.h>
#include <drivers/pci.h>

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    offset &= 0xFC; // "its for good cpu performence x86 said some dude on reddit"
                    // ~ funcieqDEV
    outl(PCI_CONFIG_ADDRESS, PCI_MAKE_ADDR(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint32_t pci_get_device_id(uint8_t bus, uint8_t slot, uint8_t func) {
    return pci_read32(bus, slot, func, 0x00);
}

int pci_device_exists(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t id = pci_get_device_id(bus, slot, func);
    return (id != 0xFFFFFFFF && (id & 0xFFFF) != 0xFFFF);
}

void pci_read_device_info(uint8_t bus, uint8_t slot, uint8_t func, pci_device_info_t* info) {
    uint32_t id = pci_read32(bus, slot, func, 0x00);
    uint32_t class_reg = pci_read32(bus, slot, func, 0x08);

    info->vendor     = id & 0xFFFF;
    info->device     = (id >> 16) & 0xFFFF;

    info->revision   = class_reg & 0xFF;
    info->prog_if    = (class_reg >> 8) & 0xFF;
    info->subclass   = (class_reg >> 16) & 0xFF;
    info->class_code = (class_reg >> 24) & 0xFF;
}

void pci_print_device(uint8_t bus, uint8_t slot, uint8_t func, const pci_device_info_t* info) {
    printk("PCI", "Device %x:%x.%u vendor=%x device=%x class=%x subclass=%x", bus, slot, func, info->vendor, info->device, info->class_code, info->subclass);
}

uint8_t pci_get_header_type(uint8_t bus, uint8_t slot, uint8_t func) {
    return (pci_read32(bus, slot, func, 0x0C) >> 16) & 0xFF;
}

int pci_is_multifunction(uint8_t bus, uint8_t slot) {
    return pci_get_header_type(bus, slot, 0) & 0x80;
}

void pci_scan_bus() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t id = pci_get_device_id(bus, slot, 0);

            if ((id & 0xFFFF) == 0xFFFF) continue;

            pci_device_info_t pci_fd_info;
            pci_read_device_info(bus, slot, 0, &pci_fd_info);
            pci_print_device(bus, slot, 0, &pci_fd_info);
            
            // if multifunction theoretically it should have 8 funcs?
            if (pci_is_multifunction(bus, slot)) {
                for (uint8_t func = 1; func < 8; func++) {
                    uint32_t id2 = pci_get_device_id(bus, slot, func);
                    pci_device_info_t device_data;
                    pci_read_device_info(bus, slot, func, &device_data);
                    if ((id2 & 0xFFFF) != 0xFFFF) pci_print_device(bus, slot, func, &device_data);
                }
            }
        }
    }
}
