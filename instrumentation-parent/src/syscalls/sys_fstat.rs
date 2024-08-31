// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output};

pub const NUM: u64 = nix::libc::SYS_fstat as u64;

make_args! {
    fd: i32,
    buf: U64AsString,
}

// We ignore buf here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, fd);

make_output! {
    return_code = return,
    data = pod(buf, super::structures::stat),
}

impl crate::FuzzyEq<Output> for Output {
    fn fuzzy_eq(&self, other: &Output) -> Option<()> {
        let self_i64 = self.return_code.0 as i64;
        let other_i64 = other.return_code.0 as i64;
        // We only compare data if its not an error, and if it is an error, the return code
        // is between 0 and -4095 (error codes), and we just make sure the same error
        // is occurring
        if (self_i64 < 0 && self_i64 >= -4095) || (other_i64 < 0 && other_i64 >= -4095) {
            // One or both is an error, compare return codes.
            (self_i64 == other_i64).then_some(())
        } else {
            // We compare with the fuzzyeq impl for stat
            self.data.fuzzy_eq(&other.data)
        }
    }
}

make_empty_mock!();
