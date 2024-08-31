// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{make_args, make_output, impl_fuzzyeq_exact_match},
    KnownSyscallArgs, MockAction,
};
use crate::TapeEntry;
use anyhow::Result;
use nix::{
    libc::{user_regs_struct, AF_INET, AF_INET6, EAFNOSUPPORT, AF_UNIX},
    unistd::Pid,
};
use serde::Deserialize;
use std::{collections::HashMap, path::Path};

pub const NUM: u64 = nix::libc::SYS_socket as u64;

make_args! {
    family: i32,
    type_socket: i32,
    protocol: i32,
}


impl_fuzzyeq_exact_match!(Args);


make_output! {
    return_code = return,
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, Deserialize, Default)]
#[serde(deny_unknown_fields, rename_all = "snake_case", default)]
pub struct Allowable {
    af_inet: bool,
    af_inet6: bool,
    af_unix: bool,
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "snake_case")]
pub struct Mock {
    allow: Allowable,
}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        _parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr : u64,
        _pid: Pid,
        mut regs: user_regs_struct,
        _args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match mock {
            Some(Mock { allow }) => {
                if allow.af_inet && (regs.rdi == (AF_INET as u64)) {
                    log::debug!("Allowing socket call through as it's domain argument is AF_INET.");
                    Ok((MockAction::DontMock, None))
                } else if allow.af_inet6 && (regs.rdi == (AF_INET6 as u64)) {
                    log::debug!(
                        "Allowing socket call through as it's domain argument is AF_INET6."
                    );
                    Ok((MockAction::DontMock, None))
                } else if allow.af_unix && (regs.rdi == (AF_UNIX as u64)) {
                    log::debug!("Allowing socket call through as it's domain argument is AF_UNIX.");
                    Ok((MockAction::DontMock, None))
                } else {
                    log::debug!(
                        "Rejecting socket as it's domain is not in the allowable set of domains."
                    );
                    regs.rax = -EAFNOSUPPORT as u64;
                    Ok((MockAction::NoOp(regs), None))
                }
            }
            None => Ok((MockAction::DontMock, None)),
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        match *self {
            Mock { .. } => Ok(()),
        }
    }
}
