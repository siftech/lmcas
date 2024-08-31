// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::{I64AsString, U64AsString};

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_pread64 as u64;

make_args! {
    fd: i32,
    buf: U64AsString,
    count: U64AsString,
    pos: I64AsString,
}

impl_fuzzyeq_partial_match!(Args, fd, count, pos);

make_output! {
    return_code = return,
    data = bytes(buf, count),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
