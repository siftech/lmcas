// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_prlimit64 as u64;

make_args! {
    pid: i32,
    resource: i32,
    nlim_ptr: U64AsString,
    olim_ptr: U64AsString,
}

// We ignore nlim_ptr,olim_ptr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, pid, resource);

make_output! {
    return_code = return,
    nlim = optional_pod(nlim_ptr, super::structures::rlimit),
    olim = optional_pod(olim_ptr, super::structures::rlimit),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();
