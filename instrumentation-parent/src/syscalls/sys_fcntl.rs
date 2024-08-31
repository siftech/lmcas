// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::{
    syscalls::{
        macros::{impl_fuzzyeq_exact_match, make_args, make_output},
        KnownSyscallArgs, MockAction,
    },
    TapeEntry, U64AsString,
};
use anyhow::{bail, Result};
use nix::{
    libc::{user_regs_struct, F_SETFD},
    unistd::Pid,
};
use std::{collections::HashMap, path::Path};

pub const NUM: u64 = nix::libc::SYS_fcntl as u64;

make_args! {
    fd: i32,
    command: u32,
    arg: U64AsString,
}

impl_fuzzyeq_exact_match!(Args);

make_output! {
    return_code = return,
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, serde::Deserialize)]
pub enum Mock {}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        _parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr: u64,
        _pid: Pid,
        _regs: user_regs_struct,
        args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match (args.command as i32, mock) {
            (F_SETFD, _) => {
                log::debug!(
                    "Allowing through call to sys_fcntl with F_SETFD on fd {}",
                    args.fd
                );
                Ok((MockAction::DontMock, None))
            }

            (F_GETFD, _) => {
                log::debug!(
                    "Allowing through call to sys_fcntl with F_GETFD on fd {}",
                    args.fd
                );
                Ok((MockAction::DontMock, None))
            }

            (F_SETFL, _) => {
                log::debug!(
                    "Allowing through call to sys_fcntl with F_SETFL on fd {}",
                    args.fd
                );
                Ok((MockAction::DontMock, None))
            }

            (F_GETFL, _) => {
                log::debug!(
                    "Allowing through call to sys_fcntl with F_GETFL on fd {}",
                    args.fd
                );
                Ok((MockAction::DontMock, None))
            }

            _ => {
                bail!(
                    "Rejecting fcntl call with command {} on fd {}, as it's not supported.",
                    args.command,
                    args.fd
                )
            }
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        Ok(())
    }
}
