use core::alloc::{GlobalAlloc, Layout};
use core::ffi::c_void;
use core::ptr;

use crate::kernel::ffi::{free, malloc};

struct KernelAllocator;

unsafe impl GlobalAlloc for KernelAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        if layout.size() == 0 {
            return layout.align() as *mut u8;
        }

        let ptr = unsafe { malloc(layout.size()) as *mut u8 };

        if ptr.is_null() {
            return ptr::null_mut();
        }

        ptr
    }

    unsafe fn dealloc(&self, ptr: *mut u8, _layout: Layout) {
        if !ptr.is_null() {
            unsafe {
                free(ptr as *mut c_void);
            }
        }
    }
}

#[alloc_error_handler]
fn alloc_error(_layout: Layout) -> ! {
    panic!(
        "Failed to allocate an required amount of memory; try to increase the system RAM available",
    );
}

#[global_allocator]
static ALLOCATOR: KernelAllocator = KernelAllocator;
