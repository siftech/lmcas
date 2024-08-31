// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_uname as u64;

make_args! {
    buf: U64AsString,
}

// No non-memory address args to compare
impl_fuzzyeq_partial_match!(Args);

make_output! {
    return_code = return,
    // 390 is the result of sizeof(utsname), the struct being written to
    // in the buf pointer by the syscall.
    data = bytes(buf, 390),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
