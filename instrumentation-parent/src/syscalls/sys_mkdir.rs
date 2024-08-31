// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_output},
    KnownSyscallArgs, MockAction,
};
use crate::{pod::read_cstring, TapeEntry, U64AsString};
use anyhow::{Context, Result};
use nix::{
    libc::{user_regs_struct, EEXIST},
    unistd::Pid,
};
use serde::Deserialize;
use std::{collections::HashMap, ffi::OsStr, fs, os::unix::ffi::OsStrExt, path::Path};

pub const NUM: u64 = nix::libc::SYS_mkdir as u64;

make_args! {
    pathname_ptr: U64AsString,
    mode: u32,
}

// We ignore pathname_ptr because its a memory address
// and can vary without issue.
impl_fuzzyeq_partial_match!(Args, mode);

make_output! {
    return_code = return,
    pathname = string(pathname_ptr),
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "snake_case", tag = "type")]
pub enum Mock {}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        _parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr: u64,
        pid: Pid,
        mut regs: user_regs_struct,
        args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match mock {
            Some(mock) => match *mock {},
            None => {
                let old_path = read_cstring(pid, args.pathname_ptr.into()).with_context(|| {
                    format!(
                        "Failed to read old path from address 0x{:016x}",
                        args.pathname_ptr.0
                    )
                })?;
                let old_path = OsStr::from_bytes(old_path.to_bytes());
                log::debug!("Creating directory {:?} and returning EEXIST.", old_path);
                fs::create_dir_all(<OsStr as AsRef<OsStr>>::as_ref(old_path))?;
                regs.rax = (-EEXIST) as u64;
                Ok((MockAction::NoOp(regs), None))
            }
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        match *self {}
    }
}
