use core::ffi::{c_char, c_void};

#[repr(C)]
#[derive(Clone, Copy)]
pub enum KernelMessageType {
    Log = 0, // KMT_LOG
    Error = 1, // KMT_ERROR
    Debug = 2, // KMT_DEBUG
    // probably should include KMT_EXTRA_DEBUG in the future 
}

#[repr(C)]
pub enum KeyboardKeyPressKind {
    Char = 0
}

#[repr(C)]
pub struct KeyboardKeyPress {
    pub kind: KeyboardKeyPressKind,
    pub char: c_char
}

unsafe extern "C" {
    #[allow(dead_code)] // remove later
    pub fn phys_to_virt(phys: usize) -> usize;
    pub fn malloc(size: usize) -> *mut c_void;
    pub fn free(ptr: *mut c_void) -> ();
    pub fn panic(msg: *const c_char, int_frame: *const c_void) -> i32;
    pub fn printf(fmt: *const c_char, ...);
    pub fn keyboard_handle_keypress(keypress: KeyboardKeyPress) -> i32;
    pub fn printk(message_type: KernelMessageType, module: *const c_char, fmt: *const c_char, ...);
}
