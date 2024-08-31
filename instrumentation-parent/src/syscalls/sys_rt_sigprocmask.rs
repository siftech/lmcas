// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_rt_sigprocmask as u64;

make_args! {
    how: i32,
    nset_ptr: U64AsString,
    oset_ptr: U64AsString,
    sigsetsize: U64AsString,
}

// We ignore act_ptr and oact_ptr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, how, sigsetsize);

make_output! {
    return_code = return,
    nset =  optional_pod(nset_ptr, super::structures::sigset_t),
    oset = optional_pod(oset_ptr, super::structures::sigset_t),
}

impl_fuzzyeq_exact_match!(Output);

make_empty_mock!();