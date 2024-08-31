// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_writev as u64;

make_args! {
    fd: i32,
    iov: U64AsString,
    iovcnt: U64AsString,
}

// We ignore iov here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, fd, iovcnt);

make_output! {
    return_code = return,
    iovs = iovs(iov, iovcnt),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
