// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use super::macros::{
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_empty_mock, make_output,
};

pub const NUM: u64 = nix::libc::SYS_getgroups as u64;

// sys_getgroups	int gidsetsize	gid_t *grouplist
make_args! {
    size: i32,
    buflist: U64AsString,
}

// We don't check the buflist address itself
// because it can vary.
impl_fuzzyeq_partial_match!(Args);

make_output! {
    return_code = return,
    data = pods(buflist, u32, size),
}

impl crate::FuzzyEq for Output {
    fn fuzzy_eq(&self, other: &Output) -> Option<()> {
        let self_ret_i64 = self.return_code.0 as i64;
        let other_ret_i64 = other.return_code.0 as i64;
        // If one of them is an error, skip comparing the buflist
        if (self_ret_i64 < 0 && self_ret_i64 >= -4095) || (other_ret_i64 < 0 && other_ret_i64 >= -4095) {
            (self_ret_i64 == other_ret_i64).then_some(())
        } else {
        // Else compare both return code and buflist listed amounts
        let self_ret_usize = self.return_code.0 as usize;
        let other_ret_usize = other.return_code.0 as usize;
        let slice_self = if self_ret_usize > self.data.len() {
            log::warn!("Somehow the size of the non-error return value for sys_getgroups is greater than the size.\
                        This is for the syscall output {:?}", self);
            &self.data[..self.data.len()]
        } else {
            &self.data[..self_ret_usize]
        };
        let slice_other = if other_ret_usize > other.data.len() {
            log::warn!("Somehow the size of the non-error return value for sys_getgroups is greater than the size.\
                        This is for the syscall output {:?}", other);
            &other.data[..other.data.len()]
        } else {
            &other.data[..other_ret_usize]
        };

        ((self_ret_usize == other_ret_usize) && (slice_self == slice_other)).then_some(())


        }
    }
}

make_empty_mock!();
