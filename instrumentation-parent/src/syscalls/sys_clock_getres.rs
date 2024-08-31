// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output, impl_fuzzyeq_exact_match};

pub const NUM: u64 = nix::libc::SYS_clock_getres as u64;

make_args! {
    which_clock: U64AsString,
    buf: U64AsString,
}

// We ignore buf here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, which_clock);

make_output! {
    return_code = return,
    data = pod(buf, super::structures::timespec),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
