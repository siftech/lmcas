// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_output},
    KnownSyscallArgs, MockAction,
};
use crate::{pod::read_pod, TapeEntry, U64AsString};
use anyhow::bail;
use nix::{
    libc::{sa_family_t, user_regs_struct, AF_UNIX, ENOENT},
    unistd::Pid,
};
use std::collections::HashMap;

pub const NUM: u64 = nix::libc::SYS_connect as u64;

make_args! {
    fd: i32,
    sockaddr_ptr: U64AsString,
    addrlen: U64AsString,
}

// We ignore sockaddr_ptr here because the memory address can vary
// despite having equivalent semantics
// and we check the contents later anyway.
impl_fuzzyeq_partial_match!(Args, fd, addrlen);

make_output! {
    return_code = return,
    sockaddr_data = bytes(sockaddr_ptr, addrlen),
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, serde::Deserialize)]
pub enum Mock {}

impl Mock {
    pub fn check_syscall_for_mocking(
        _mock: Option<&Mock>,
        _parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        _noop_sighandler_addr: u64,
        pid: Pid,
        mut regs: user_regs_struct,
        args: &Args,
    ) -> anyhow::Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        let sa_family = read_pod::<sa_family_t>(pid, args.sockaddr_ptr.into())?;
        if sa_family == AF_UNIX as u16 {
            log::debug!("Returning ENOENT for connect as its using address family AF_UNIX.");
            // We explicitly return ENOENT for af_unix
            regs.rax = -ENOENT as u64;
            Ok((MockAction::NoOp(regs), None))
        } else {
            bail!("Connect was called with an unsupported address family.")
        }
    }

    pub fn setup_and_validate(
        &mut self,
        _relative_path_dir: &std::path::Path,
    ) -> anyhow::Result<()> {
        match *self {}
    }
}
