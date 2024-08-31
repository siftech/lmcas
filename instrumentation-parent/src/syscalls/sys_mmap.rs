// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::{I64AsString, U64AsString};

use super::macros::{impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output};

pub const NUM: u64 = nix::libc::SYS_mmap as u64;

make_args! {
    addr: U64AsString,
    len: U64AsString,
    prot: i32,
    flags: i32,
    fd: i32,
    off: I64AsString,
}

impl_fuzzyeq_partial_match!(Args, len, prot, flags, fd, off);

make_output! {
    return_code = return,
}

//Custom handling here as the return code comparison needs to
// be cognizant of mmap returning an address.
impl crate::FuzzyEq for Output {
    fn fuzzy_eq(&self, other: &Output) -> Option<()> {
        let self_i64 = self.return_code.0 as i64;
        let other_i64 = other.return_code.0 as i64;
        // We only compare return code if its between 0 and -4095 (error codes)
        // return codes outside of that range are addresses in mmap, which
        // we cannot feasibly compare.
        if (self_i64 < 0 && self_i64 >= -4095) || (other_i64 < 0 && other_i64 >= -4095) {
            (self_i64 == other_i64).then_some(())
        } else {
            Some(())
        }
    }
}

make_empty_mock!();
