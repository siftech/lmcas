// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::I64AsString;

use super::macros::{impl_fuzzyeq_exact_match, make_args, make_empty_mock, make_output};

pub const NUM: u64 = nix::libc::SYS_lseek as u64;

make_args! {
    fd: i32,
    offset: I64AsString,
    origin: u32,
}

impl_fuzzyeq_exact_match!(Args);

make_output! {
    return_code = return,
}

make_empty_mock!();

impl_fuzzyeq_exact_match!(Output);
