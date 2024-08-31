// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{make_args, make_empty_mock, make_output, impl_fuzzyeq_partial_match, impl_fuzzyeq_exact_match};

pub const NUM: u64 = nix::libc::SYS_munmap as u64;

make_args! {
    addr: U64AsString,
    len: U64AsString,
}


// We ignore addr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, len);

make_output! {
    return_code = return,
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
