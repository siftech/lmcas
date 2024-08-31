// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{impl_fuzzyeq_partial_match, make_args, make_output},
    KnownSyscallArgs, MockAction,
};
use crate::{
    pod::{read_cstring, write_bytes},
    TapeEntry, U64AsString,
};
use anyhow::{ensure, Context, Result};
use nix::{libc::user_regs_struct, unistd::Pid};
use serde::Deserialize;
use std::{
    collections::HashMap,
    ffi::{CString, OsStr},
    os::unix::ffi::OsStrExt,
    path::Path,
};

pub const NUM: u64 = nix::libc::SYS_stat as u64;

make_args! {
    filename_ptr: U64AsString,
    buf: U64AsString,
}

// No input args to compare that aren't memory addresses.
impl_fuzzyeq_partial_match!(Args);

make_output! {
    return_code = return,
    filename = string(filename_ptr),
    data = pod(buf, super::structures::stat),
}

impl crate::FuzzyEq<Output> for Output {
    fn fuzzy_eq(&self, other: &Output) -> Option<()> {
        let self_i64 = self.return_code.0 as i64;
        let other_i64 = other.return_code.0 as i64;
        // We only compare data if its not an error, and if it is an error, the return code
        // is between 0 and -4095 (error codes), and we just make sure the same error
        // is occurring
        if (self_i64 < 0 && self_i64 >= -4095) || (other_i64 < 0 && other_i64 >= -4095) {
            // One or both is an error, compare return codes.
            (self_i64 == other_i64).then_some(())
        } else {
            // We compare with the fuzzyeq impl for stat
            self.data.fuzzy_eq(&other.data)
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Mock {
    mapping: HashMap<CString, CString>,
}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr: u64,
        pid: Pid,
        mut regs: user_regs_struct,
        args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match mock {
            Some(Mock { ref mapping }) => {
                let old_path = read_cstring(pid, args.filename_ptr.into()).with_context(|| {
                    format!(
                        "Failed to read old path from address 0x{:016x}",
                        args.filename_ptr.0
                    )
                })?;

                if let Some(new_path) = mapping.get(&old_path) {
                    // Write new_path to the parent page.
                    assert_eq!(parent_page_addr & 0b111, 0);
                    assert!(new_path.as_bytes_with_nul().len() <= 4096);

                    log::debug!("Mocking open({:?}, ...) to {:?}", old_path, new_path);

                    write_bytes(pid, parent_page_addr, new_path.as_bytes_with_nul())
                        .context("Failed to write new path to child's memory")?;

                    regs.rdi = parent_page_addr;
                    Ok((MockAction::Replace(regs), None))
                } else {
                    log::debug!("Not mocking open({:?}, ...)", old_path);
                    Ok((MockAction::DontMock, None))
                }
            }
            None => Ok((MockAction::DontMock, None)),
        }
    }

    pub fn setup_and_validate(&mut self, relative_path_dir: &Path) -> Result<()> {
        match *self {
            Mock { ref mut mapping } => {
                for (old_path, new_path) in mapping {
                    ensure!(old_path.as_bytes_with_nul().len() <= 4096);

                    let new_path_osstr = OsStr::from_bytes(new_path.as_bytes());
                    let new_path_absolute = relative_path_dir.join(new_path_osstr);
                    *new_path = CString::new(new_path_absolute.as_os_str().as_bytes())
                        .context("Path had a null byte")?;

                    ensure!(new_path.as_bytes_with_nul().len() <= 4096);
                }
                Ok(())
            }
        }
    }
}
