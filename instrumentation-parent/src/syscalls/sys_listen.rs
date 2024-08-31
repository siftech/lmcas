// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::macros::{impl_fuzzyeq_exact_match, make_args, make_empty_mock, make_output};

pub const NUM: u64 = nix::libc::SYS_listen as u64;

make_args! {
    fd: i32,
    backlog: i32,
}

impl_fuzzyeq_exact_match!(Args);

make_output! {
    return_code = return,
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
