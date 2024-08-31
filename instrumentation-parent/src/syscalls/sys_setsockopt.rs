// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_setsockopt as u64;

make_args! {
    fd: i32,
    level: i32,
    optname: i32,
    optval_ptr: U64AsString,
    optlen: i32,
}

// We ignore iov here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, fd, level, optname, optlen);

make_output! {
    return_code = return,
    optval = string(optval_ptr),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
