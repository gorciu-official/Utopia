use alloc::boxed::Box;
use alloc::vec::Vec;
use core::ffi::c_void;
use core::slice;
use structured_zstd::decoding::StreamingDecoder;
use structured_zstd::io::Read;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct TarfsImage {
    pub data: *const u8,
    pub size: u64,
}

unsafe extern "C" {
    static mut tarfs_image: TarfsImage;
}

#[unsafe(no_mangle)]
extern "C" fn tarfs_set_image(data: *const c_void, size: u64) {
    // TODO: this is vulnerable to timing, either when an interrupt
    // fires / another thread wants something so fix this

    // TODO: allow uncompressed image to be loaded too

    let mut compressed = unsafe { slice::from_raw_parts(data.cast::<u8>(), size as usize) };
    let mut decoder = StreamingDecoder::new(&mut compressed);

    let mut decompressed = Vec::new();
    decoder.read_to_end(&mut decompressed).unwrap(); // peak safety

    let leaked = Box::leak(decompressed.into_boxed_slice());

    unsafe {
        tarfs_image = TarfsImage {
            data: leaked.as_ptr(),
            size: leaked.len() as u64,
        };
    }
}
