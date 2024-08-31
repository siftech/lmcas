// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::macros::{impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output};
use crate::U64AsString;

pub const NUM: u64 = nix::libc::SYS_brk as u64;

make_args! {
    brk: U64AsString,
}

// We ignore brk here because the memory address can vary
// despite having equivalent semantics.
impl_fuzzyeq_partial_match!(Args);

make_output! {
    return_code = return,
}

// We ignore the return_code here because the memory address can vary
// despite having equivalent semantics.
impl_fuzzyeq_partial_match!(Output);

make_empty_mock!();
