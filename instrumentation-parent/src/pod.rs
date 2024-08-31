// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use anyhow::{Context, Result};
use nix::{libc::iovec, sys::ptrace, unistd::Pid};
use std::{
    any::type_name,
    ffi::{c_void, CString},
    mem::{size_of, size_of_val, MaybeUninit},
    num::NonZeroU8,
    ptr,
};

/// A trait promising that the type is "plain old data," that is, it's copyable and any bit pattern
/// of the correct size is a valid value of the type.
///
/// Examples of types that are *not* Pod:
///
/// - `String`: This owns memory, so is not `Copy`
/// - `NonZeroU8`: Not every bit pattern is a valid `NonZeroU8`
/// - `(u8, u16)`: Writes to the padding bytes result in UB.
pub unsafe trait Pod: Copy + Sized {}

unsafe impl Pod for u64 {}

unsafe impl Pod for u32 {}
unsafe impl Pod for i32 {}

unsafe impl Pod for u16 {}
unsafe impl Pod for iovec {}

unsafe impl Pod for (u64, u64) {}

/// Reads a `Pod` type from the tracee's memory.
pub fn read_pod<T: Pod>(pid: Pid, addr: u64) -> Result<T> {
    let bytes = read_bytes(pid, addr, size_of::<T>() as u64).with_context(|| {
        format!(
            "Failed to read {} from address {:016x}",
            type_name::<T>(),
            addr
        )
    })?;
    let mut value = MaybeUninit::<T>::uninit();

    // Even though the read_bytes function should always return the right number of bytes, it
    // doesn't hurt to have an extra check.
    assert_eq!(bytes.len(), size_of_val(&value));

    // UNSAFE: bytes is valid to read from and value is valid to write to by the given amount, as
    // checked by the assertion. Since we're using u8 as ptr::copy's T, any alignment is OK.
    unsafe {
        ptr::copy(&bytes[0], value.as_mut_ptr() as *mut u8, bytes.len());
    }

    // UNSAFE: value gets initialized to particular bytes by the above ptr::copy. We know these
    // bytes must be valid, because T impls Pod, which requires that any bit pattern is valid.
    let value = unsafe { value.assume_init() };

    Ok(value)
}

pub fn read_pod_option<T: Pod>(pid: Pid, addr: u64) -> Option<T> {
    if addr != 0 {
        read_pod(pid, addr).ok()
    } else {
        None
    }
}

/// Reads a `Vec<u8>` from the tracee's memory.
///
/// The resulting vector has exactly `len` items.
pub fn read_bytes(pid: Pid, addr: u64, len: u64) -> Result<Vec<u8>> {
    if len == 0 {
        return Ok(Vec::new());
    }
    peek_data_iter(pid, addr)
        .and_then(|iter| iter.take(len as usize).collect())
        .with_context(|| format!("Failed to read {} bytes from address {:016x}", len, addr))
}

/// Writes a `&[u8]` to the tracee's memory.
pub fn write_bytes(pid: Pid, mut addr: u64, mut bytes: &[u8]) -> Result<()> {
    // There might be an initial unaligned portion; if so, write it out.
    if addr & 7 != 0 {
        let unaligned_start_bytes_len = (8 - (addr & 7)) as usize;
        let unaligned_start_bytes = &bytes[..unaligned_start_bytes_len];

        write_partial_word(pid, addr, unaligned_start_bytes)
            .context("Failed to write initial part of a byte slice")?;

        addr += unaligned_start_bytes_len as u64;
        bytes = &bytes[unaligned_start_bytes_len..];
    }

    // Write everything that we can write in terms of words.
    let aligned_bytes_len = bytes.len() & !7;
    write_bytes_as_words(pid, addr, &bytes[..aligned_bytes_len])
        .context("Failed to write a byte slice")?;

    addr += aligned_bytes_len as u64;
    bytes = &bytes[aligned_bytes_len..];

    // If there's a final portion, write that too.
    if !bytes.is_empty() {
        write_partial_word(pid, addr, bytes)
            .context("Failed to write final part of a byte slice")?;
    }

    Ok(())
}

/// Writes a sequence of words to a word-aligned address.
fn write_bytes_as_words(pid: Pid, mut addr: u64, mut bytes: &[u8]) -> Result<()> {
    assert_eq!(addr & 7, 0);
    assert_eq!(bytes.len() & 7, 0);

    while !bytes.is_empty() {
        let data = (bytes[0] as u64)
            | (bytes[1] as u64) << 8
            | (bytes[2] as u64) << 16
            | (bytes[3] as u64) << 24
            | (bytes[4] as u64) << 32
            | (bytes[5] as u64) << 40
            | (bytes[6] as u64) << 48
            | (bytes[7] as u64) << 56;

        unsafe { ptrace::write(pid, addr as *mut c_void, data as *mut c_void) }
            .with_context(|| format!("Failed to write to address {:016x}", addr))?;

        addr += 8;
        bytes = &bytes[8..];
    }
    Ok(())
}

/// Writes a less-than-word-sized chunk of bytes to a potentially unaligned address. If the address
/// is unaligned, the number of bytes must be less than the room remaining to the word boundary.
fn write_partial_word(pid: Pid, addr: u64, bytes: &[u8]) -> Result<()> {
    let addr_aligned_down = addr & !7;
    let addr_boundary = addr_aligned_down + 8;
    let remaining_bytes = addr_boundary - addr;
    assert!(bytes.len() as u64 <= remaining_bytes);

    let data = ptrace::read(pid, addr_aligned_down as *mut c_void)
        .with_context(|| format!("Failed to read from address {:016x}", addr_aligned_down))?;

    let mut data_bytes = [0; 8];
    data_bytes[0] = data as u8;
    data_bytes[1] = (data >> 8) as u8;
    data_bytes[2] = (data >> 16) as u8;
    data_bytes[3] = (data >> 24) as u8;
    data_bytes[4] = (data >> 32) as u8;
    data_bytes[5] = (data >> 40) as u8;
    data_bytes[6] = (data >> 48) as u8;
    data_bytes[7] = (data >> 56) as u8;

    let offset = (addr & 7) as usize;
    data_bytes[offset..offset + bytes.len()].copy_from_slice(bytes);

    let data = (data_bytes[0] as u64)
        | (data_bytes[1] as u64) << 8
        | (data_bytes[2] as u64) << 16
        | (data_bytes[3] as u64) << 24
        | (data_bytes[4] as u64) << 32
        | (data_bytes[5] as u64) << 40
        | (data_bytes[6] as u64) << 48
        | (data_bytes[7] as u64) << 56;

    unsafe { ptrace::write(pid, addr_aligned_down as *mut c_void, data as *mut c_void) }
        .with_context(|| format!("Failed to write to address {:016x}", addr_aligned_down))
}

/// Reads multiple copies of a `Pod` type from the tracee's memory. Returns an infinite-length
/// iterator; use `.take()` to get the length you want if it is statically known, or
/// `.take_while()` or a similar method if not.
pub fn read_pods<T: Pod>(pid: Pid, mut addr: u64) -> impl Iterator<Item = Result<T>> {
    std::iter::repeat_with(move || {
        let value = read_pod(pid, addr);
        addr += std::mem::size_of::<T>() as u64;
        value
    })
}

/// Reads a `CString` from the tracee's memory.
pub fn read_cstring(pid: Pid, addr: u64) -> Result<CString> {
    // TODO: This could be much shorter with .map_while(), if we bump MSRV.
    let mut out = Vec::new();
    for result in peek_data_iter(pid, addr)? {
        match NonZeroU8::new(result?) {
            Some(byte) => out.push(byte),
            None => break,
        }
    }
    Ok(CString::from(out))
}

/// Reads a `Vec<u8>` from the tracee's memory.
///
/// The resulting vector has exactly `len` items.
pub fn read_iovs(pid: Pid, addr: u64, len: u64) -> Result<Vec<(iovec, Vec<u8>)>> {
    let sizeof_iovec = size_of::<iovec>() as u64;

    (0..len)
        .map(|i| addr + i * sizeof_iovec)
        .map(|iov_addr| {
            let iov: iovec = read_pod(pid, iov_addr).context("Failed reading base iovec")?;
            let addr = iov.iov_base as u64;
            let len = iov.iov_len as u64;
            let bytes = read_bytes(pid, addr, len).context("Failed reading iovec data")?;
            Ok((iov, bytes))
        })
        .collect()
}

/// Returns a `PeekDataIter` that reads from the tracee's memory.
fn peek_data_iter(pid: Pid, addr: u64) -> Result<impl Iterator<Item = Result<u8>>> {
    PeekDataIter::new(addr, move |addr| {
        let word = ptrace::read(pid, addr as *mut c_void)?;
        Ok(word as u64)
    })
}

/// An iterator for reading bytes out of the tracee's memory. Generic over a reader function for
/// easier testing. (This gets monomorphized away anyway.)
struct PeekDataIter<F: Fn(u64) -> Result<u64>> {
    // The function used to read a word from the tracee's memory.
    read_word: F,

    // The bytes of the word that we're currently reading.
    current_word: u64,

    // The next address we'll read a word from (into current_word).
    next_addr: u64,

    // The index (into the bytes of current_word) we'll return next from the iterator.
    next_index: u64,
}

impl<F: Fn(u64) -> Result<u64>> PeekDataIter<F> {
    /// Create a new `PeekDataIter`, reading the first word.
    fn new(addr: u64, read_word: F) -> Result<PeekDataIter<F>> {
        let addr_aligned_down = addr & !7;
        let current_word = read_word(addr_aligned_down)?;
        Ok(PeekDataIter {
            read_word,
            current_word,
            next_addr: addr_aligned_down + 8,
            next_index: addr - addr_aligned_down,
        })
    }
}

impl<F: Fn(u64) -> Result<u64>> Iterator for PeekDataIter<F> {
    type Item = Result<u8>;

    fn next(&mut self) -> Option<Result<u8>> {
        if self.next_index == 8 {
            match (self.read_word)(self.next_addr) {
                Ok(word) => self.current_word = word,
                Err(err) => return Some(Err(err)),
            }
            self.next_addr += 8;
            self.next_index = 0;
        }

        let out = (self.current_word >> (8 * self.next_index)) as u8;
        self.next_index += 1;
        Some(Ok(out))
    }
}

#[test]
fn test_read_bytes() {
    use std::cell::RefCell;

    let counter = RefCell::new(0);

    // A read_word function that acts as if memory contains the low byte of the address at each
    // byte.
    let mock_read_word = |addr: u64| {
        // Requests to the real PTRACE_PEEKDATA need to be word-aligned.
        assert_eq!(addr & 0x7, 0);

        *counter.borrow_mut() += 1;

        let low_byte = addr as u8;
        let mut out = 0;
        for i in 0..8 {
            out |= ((low_byte + i) as u64) << (i * 8);
        }
        Ok(out)
    };

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x108a, mock_read_word)
            .unwrap()
            .take(3)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\x8a\x8b\x8c"
    );
    assert_eq!(*counter.borrow(), 1);

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x10ff, mock_read_word)
            .unwrap()
            .take(2)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\xff\x00"
    );
    assert_eq!(*counter.borrow(), 2);

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x1030, mock_read_word)
            .unwrap()
            .take(16)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f",
    );
    assert_eq!(*counter.borrow(), 2);

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x1030, mock_read_word)
            .unwrap()
            .take(18)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f\x40\x41",
    );
    assert_eq!(*counter.borrow(), 3);

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x102f, mock_read_word)
            .unwrap()
            .take(17)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\x2f\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f",
    );
    assert_eq!(*counter.borrow(), 3);

    *counter.borrow_mut() = 0;
    assert_eq!(
        PeekDataIter::new(0x1003, mock_read_word)
            .unwrap()
            .take(10)
            .collect::<Result<Vec<_>>>()
            .unwrap(),
        b"\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"
    );
    assert_eq!(*counter.borrow(), 2);
}
