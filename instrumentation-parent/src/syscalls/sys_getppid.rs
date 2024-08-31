// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::macros::{impl_fuzzyeq_exact_match, make_args, make_output, make_return_value_mock};

pub const NUM: u64 = nix::libc::SYS_getppid as u64;

make_args! {}

impl_fuzzyeq_exact_match!(Args);

make_output! {
    return_code = return,
}

impl_fuzzyeq_exact_match!(Output);

make_return_value_mock!(ppid);
