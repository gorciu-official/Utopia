#![cfg(target_arch = "x86_64")]
#![cfg(CONFIG_PCI)]

// Based of https://osdev.wiki/wiki/PCI
// THIS IS NOT A COMPLETE PCI DRIVER IT ONLY SCANS FOR PCI DEVICES AND PRINTS THEM OUT AS LOGS

use crate::arch::pmio::{arch_inl, arch_outl};
use crate::kernel::ffi::{KernelMessageType::Log, printk};
use alloc::vec::Vec;

const PCI_CONFIG_ADDRESS: u16 = 0xCF8;
const PCI_CONFIG_DATA: u16 = 0xCFC;

#[derive(Debug, Clone, Copy)]
pub struct PciDevice {
    pub bus: u8,
    pub device: u8,
    pub function: u8,

    pub vendor_id: u16,
    pub device_id: u16,

    pub header_type: u8,
}

pub unsafe fn pci_config_read_word(bus: u8, device: u8, function: u8, offset: u8) -> u16 {
    let address: u32 = 0x80000000
        | ((bus as u32) << 16)
        | ((device as u32) << 11)
        | ((function as u32) << 8)
        | ((offset as u32) & 0xFC);

    unsafe {
        arch_outl(PCI_CONFIG_ADDRESS, address);
    }

    let tmp = (unsafe { arch_inl(PCI_CONFIG_DATA) } >> ((offset & 2) * 8) & 0xFFFF) as u16;
    tmp
}

pub unsafe fn pci_device(bus: u8, device: u8, function: u8) -> Option<PciDevice> {
    let vendor_id = unsafe { pci_config_read_word(bus, device, function, 0x00) };

    if vendor_id == 0xFFFF {
        return None;
    }

    let device_id = unsafe { pci_config_read_word(bus, device, function, 0x02) };

    let header_type = unsafe { pci_config_read_word(bus, device, function, 0x0E) } as u8;

    Some(PciDevice {
        bus,
        device,
        function,

        vendor_id,
        device_id,

        header_type,
    })
}

pub unsafe fn pci_scan() -> Vec<PciDevice> {
    let mut devices = Vec::new();

    for bus in 0..=255u8 {
        for device in 0..32u8 {
            let Some(pcidevice) = (unsafe { pci_device(bus, device, 0) }) else {
                continue;
            };

            devices.push(pcidevice);

            if (pcidevice.header_type & 0x80) != 0 {
                for function in 1..8u8 {
                    if let Some(device) = unsafe { pci_device(bus, device, function) } {
                        devices.push(device);
                    }
                }
            }
        }
    }

    devices
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn pci_log() {
    let devices = unsafe { pci_scan() };

    for dev in devices.iter() {
        unsafe {
            printk(
                Log,
                c"PCI".as_ptr(),
                c"Bus:%x Device:%x Function:%x Vendor: %x Device: %x".as_ptr(),
                dev.bus as u32,
                dev.device as u32,
                dev.function as u32,
                dev.vendor_id as u32,
                dev.device_id as u32,
            );
        }
    }
}
