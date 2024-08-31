// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_openat as u64;

make_args! {
    dirfd: i32,
    filename_ptr: U64AsString,
    flags: i32,
    mode: i32,
}

impl_fuzzyeq_partial_match!(Args, dirfd, flags, mode);

make_output! {
    return_code = return,
    filename = string(filename_ptr),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
