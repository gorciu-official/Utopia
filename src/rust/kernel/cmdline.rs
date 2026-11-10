// this is a VIBECODED kernel command parser, i know that it is VIBECODED
// and i did that because i was LAZY
// maybe somewhen i'll rewrite it

// Kernel command line parser.
//
// Syntax (arguments are separated by spaces or tabs):
//
//     attrname=attrvalue
//     attrname="attribute value with spaces"
//     attrname                 (bare flag, no value)
//
// Inside a quoted value `\"` is a literal quote and `\\` is a literal
// backslash; any other backslash is kept as is.
//
// The parser itself (`parse`) is pure Rust and only depends on `alloc`. The
// C-facing API at the bottom hands out an opaque `cmdline_t*` and NUL-terminated
// strings that stay valid until `cmdline_free` is called.
//
// C header: include/lib/cmdline.h

use alloc::boxed::Box;
use alloc::vec::Vec;
use core::ffi::{c_char, c_int, CStr};
use core::ptr;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum CmdlineError {
    NullPointer = -1,
    UnterminatedQuote = -2,
    InvalidName = -3,
    UnexpectedQuote = -4,
}

#[derive(Debug, PartialEq, Eq)]
struct Entry {
    name: Vec<u8>,
    value: Option<Vec<u8>>,
}

impl Entry {
    fn name_bytes(&self) -> &[u8] {
        &self.name[..self.name.len() - 1]
    }
}

pub struct Cmdline {
    entries: Vec<Entry>,
}

#[inline]
fn is_sep(b: u8) -> bool {
    b == b' ' || b == b'\t'
}

fn nul_terminated(bytes: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(bytes.len() + 1);
    v.extend_from_slice(bytes);
    v.push(0);
    v
}

fn parse(input: &[u8]) -> Result<Cmdline, CmdlineError> {
    let mut entries: Vec<Entry> = Vec::new();
    let mut i = 0;

    while i < input.len() {
        if is_sep(input[i]) {
            i += 1;
            continue;
        }

        let name_start = i;
        while i < input.len() && input[i] != b'=' && !is_sep(input[i]) {
            if input[i] == b'"' {
                return Err(CmdlineError::InvalidName);
            }
            i += 1;
        }
        let name = &input[name_start..i];
        if name.is_empty() {
            return Err(CmdlineError::InvalidName);
        }

        if i >= input.len() || is_sep(input[i]) {
            entries.push(Entry { name: nul_terminated(name), value: None });
            continue;
        }

        i += 1;

        let mut value: Vec<u8> = Vec::new();
        if i < input.len() && input[i] == b'"' {
            i += 1;
            let mut closed = false;
            while i < input.len() {
                match input[i] {
                    b'"' => {
                        closed = true;
                        i += 1;
                        break;
                    }
                    b'\\' if i + 1 < input.len() && matches!(input[i + 1], b'"' | b'\\') => {
                        value.push(input[i + 1]);
                        i += 2;
                    }
                    b => {
                        value.push(b);
                        i += 1;
                    }
                }
            }
            if !closed {
                return Err(CmdlineError::UnterminatedQuote);
            }
            // `a="b"c` is ambiguous, reject it
            if i < input.len() && !is_sep(input[i]) {
                return Err(CmdlineError::UnexpectedQuote);
            }
        } else {
            let start = i;
            while i < input.len() && !is_sep(input[i]) {
                if input[i] == b'"' {
                    return Err(CmdlineError::UnexpectedQuote);
                }
                i += 1;
            }
            value.extend_from_slice(&input[start..i]);
        }

        value.push(0);
        entries.push(Entry { name: nul_terminated(name), value: Some(value) });
    }

    Ok(Cmdline { entries })
}

impl Cmdline {
    fn lookup(&self, name: &[u8]) -> Option<Option<&[u8]>> {
        self.entries
            .iter()
            .rev()
            .find(|e| e.name_bytes() == name)
            .map(|e| e.value.as_deref())
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_parse(input: *const c_char, out: *mut *mut Cmdline) -> c_int {
    if out.is_null() {
        return CmdlineError::NullPointer as c_int;
    }
    unsafe { *out = ptr::null_mut() };
    if input.is_null() {
        return CmdlineError::NullPointer as c_int;
    }

    let bytes = unsafe { CStr::from_ptr(input) }.to_bytes();
    match parse(bytes) {
        Ok(cl) => {
            unsafe { *out = Box::into_raw(Box::new(cl)) };
            0
        }
        Err(e) => e as c_int,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_free(cl: *mut Cmdline) {
    if !cl.is_null() {
        drop(unsafe { Box::from_raw(cl) });
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_count(cl: *const Cmdline) -> usize {
    unsafe { cl.as_ref() }.map_or(0, |c| c.entries.len())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_name_at(cl: *const Cmdline, index: usize) -> *const c_char {
    unsafe { cl.as_ref() }
        .and_then(|c| c.entries.get(index))
        .map_or(ptr::null(), |e| e.name.as_ptr() as *const c_char)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_value_at(cl: *const Cmdline, index: usize) -> *const c_char {
    unsafe { cl.as_ref() }
        .and_then(|c| c.entries.get(index))
        .and_then(|e| e.value.as_ref())
        .map_or(ptr::null(), |v| v.as_ptr() as *const c_char)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_get(cl: *const Cmdline, name: *const c_char) -> *const c_char {
    if name.is_null() {
        return ptr::null();
    }
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    match unsafe { cl.as_ref() }.and_then(|c| c.lookup(name)) {
        Some(Some(v)) => v.as_ptr() as *const c_char,
        _ => ptr::null(),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn cmdline_has(cl: *const Cmdline, name: *const c_char) -> c_int {
    if name.is_null() {
        return 0;
    }
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    unsafe { cl.as_ref() }.and_then(|c| c.lookup(name)).is_some() as c_int
}

#[unsafe(no_mangle)]
pub extern "C" fn cmdline_strerror(code: c_int) -> *const c_char {
    let s: &'static CStr = match code {
        0 => c"ok",
        -1 => c"null pointer",
        -2 => c"unterminated quote",
        -3 => c"invalid argument name",
        -4 => c"unexpected quote",
        _ => c"unknown error",
    };
    s.as_ptr()
}
