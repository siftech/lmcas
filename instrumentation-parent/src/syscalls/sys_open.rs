// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{make_args, make_output, impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match},
    KnownSyscallArgs, MockAction,
};
use crate::{
    pod::{read_cstring, write_bytes},
    TapeEntry, U64AsString,
};
use anyhow::{ensure, Context, Result};
use nix::{
    libc::{user_regs_struct, O_WRONLY},
    unistd::Pid,
};
use serde::Deserialize;
use std::{
    collections::HashMap,
    ffi::{CString, OsStr},
    os::unix::ffi::OsStrExt,
    path::Path,
};

pub const NUM: u64 = nix::libc::SYS_open as u64;

make_args! {
    filename_ptr: U64AsString,
    flags: u32,
    mode: u32,
}

// We ignore filename_ptr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args,flags,mode);

make_output! {
    return_code = return,
    filename = string(filename_ptr),
}

impl_fuzzyeq_exact_match!(Output);


#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Mock {
    mapping: HashMap<CString, CString>,

    #[serde(default)]
    make_wronly_files_devnull: bool,
}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr : u64,
        pid: Pid,
        mut regs: user_regs_struct,
        args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match mock {
            Some(Mock {
                ref mapping,
                make_wronly_files_devnull,
            }) => {
                let old_path = read_cstring(pid, args.filename_ptr.into()).with_context(|| {
                    format!(
                        "Failed to read old path from address 0x{:016x}",
                        args.filename_ptr.0
                    )
                })?;

                let wronly_file_applies =
                    *make_wronly_files_devnull && ((regs.rsi & (O_WRONLY as u64)) != 0);
                // If we find ourselves opening a file as O_WRONLY, (and the
                // user says we should do that) we mock its filename to /dev/null
                if wronly_file_applies {
                    let new_path = CString::new("/dev/null")?;
                    // Write new_path to the parent page.
                    assert_eq!(parent_page_addr & 0b111, 0);
                    assert!(new_path.as_bytes_with_nul().len() <= 4096);
                    log::debug!("Mocking open({:?}, ...) to {:?}", old_path, new_path);
                    write_bytes(pid, parent_page_addr, new_path.as_bytes_with_nul())
                        .context("Failed to write new path to child's memory")?;
                }

                if let Some(new_path) = mapping.get(&old_path) {
                    // Write new_path to the parent page.
                    assert_eq!(parent_page_addr & 0b111, 0);
                    assert!(new_path.as_bytes_with_nul().len() <= 4096);
                    if wronly_file_applies {
                        log::warn!(
                            "You've set both make_wronly_files_devnull
                              (which applies to this file: {:?}), and created
                              a file mapping for it. Mapping this file to
                              something else is ill-advised as it will
                              likely be written to during specialization.",
                            new_path
                        );
                    }

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
            Mock {
                ref mut mapping, ..
            } => {
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
