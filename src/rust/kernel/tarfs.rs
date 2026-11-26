use alloc::vec::Vec;
use core::ffi::c_void;
use core::slice;
use structured_zstd::decoding::StreamingDecoder;
use structured_zstd::io::Read;

use crate::kernel::ffi::{KernelMessageType, printk};

#[repr(C)]
#[derive(Copy, Clone)]
pub struct TarfsImage {
    pub data: *const u8,
    pub size: u64,
}

unsafe extern "C" {
    static mut tarfs_image: TarfsImage;
}

const ZSTD_MAGIC: u32 = 0xFD_2F_B5_28;

#[unsafe(no_mangle)]
extern "C" fn tarfs_set_image(data: *const c_void, size: u64) {
    // TODO: this is vulnerable to timing, either when an interrupt
    // fires / another thread wants something so fix this

    let mut raw = unsafe { slice::from_raw_parts(data.cast::<u8>(), size as usize) };

    if !raw.starts_with(&ZSTD_MAGIC.to_le_bytes()) {
        unsafe {
            tarfs_image = TarfsImage {
                data: data.cast::<u8>(),
                size,
            };
        }
        return;
    }

    let mut decoder = StreamingDecoder::new(&mut raw);

    let mut decompressed = Vec::with_capacity(size as usize);
    match decoder.read_to_end(&mut decompressed) {
        Ok(_) => {}
        Err(_e) => unsafe {
            printk(
                KernelMessageType::Error,
                c"TARFS".as_ptr(),
                c"error when decompressing initramfs!".as_ptr(),
            )
        },
    }

    let ptr = decompressed.as_ptr();
    let len = decompressed.len() as u64;

    core::mem::forget(decompressed);

    unsafe {
        tarfs_image = TarfsImage {
            data: ptr,
            size: len,
        };
    }
}
