// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use super::{
    macros::{impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_output},
    structures::{sockaddr_in, sockaddr_in6},
    KnownSyscallArgs, MockAction,
};
use crate::{pod::read_pod, TapeEntry, U64AsString};
use anyhow::{bail, Result};
use nix::{
    libc::{sa_family_t, user_regs_struct, AF_INET, AF_INET6, EINVAL},
    unistd::Pid,
};
use serde::Deserialize;
use std::{collections::HashMap, path::Path};

pub const NUM: u64 = nix::libc::SYS_bind as u64;

make_args! {
    fd: i32,
    sockaddr_ptr: U64AsString,
    addrlen: U64AsString,
}

// We ignore sockaddr_ptr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, fd, addrlen);

make_output! {
    return_code = return,
    sockaddr_data = bytes(sockaddr_ptr,addrlen),
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, Deserialize, Default)]
#[serde(deny_unknown_fields, rename_all = "snake_case", tag = "type", default)]
pub struct Allowable {
    af_inet: bool,
    af_inet6: bool,
}

#[derive(Debug, Deserialize, Default)]
#[serde(deny_unknown_fields, rename_all = "snake_case", tag = "type", default)]
pub struct Mock {
    allow: Allowable,
}

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
            Some(Mock {
                allow:
                    Allowable {
                        af_inet: inet,
                        af_inet6: inet6,
                    },
            }) => {
                let sa_family = read_pod::<sa_family_t>(pid, args.sockaddr_ptr.into())?;
                if *inet && (sa_family == AF_INET as u16) {
                    // Now we check if the port is 0, as that allows
                    // the kernel to set one.
                    let sockaddr = read_pod::<sockaddr_in>(pid, args.sockaddr_ptr.into())?;
                    if sockaddr.sin_port == 0 {
                        // We reject it as we cant tell what the kernel will select
                        // and we could specialize based on circumstances out of our
                        // control
                        // Because this could result in incorrect programs, we fail with an error here.
                        bail!("Rejecting bind call as it's an AF_INET call with port 0.");
                    } else {
                        // We allow as usual if the port is ok.
                        log::debug!("Allowing through call to bind with address family af_inet, sockaddr: {:?}", sockaddr);
                        Ok((MockAction::DontMock, None))
                    }
                } else if *inet6 && (sa_family == AF_INET6 as u16) {
                    // Now we check if the port is 0, as that allows
                    // the kernel to set one.
                    let sockaddr = read_pod::<sockaddr_in6>(pid, args.sockaddr_ptr.into())?;
                    if sockaddr.sin6_port == 0 {
                        // We reject it as we cant tell what the kernel will select
                        // and we could specialize based on circumstances out of our
                        // control
                        // Because this could result in incorrect programs, we fail with an error here.
                        bail!("Rejecting bind call as it's an AF_INET6 call with port 0.");
                    } else {
                        // We allow as usual if the port is ok.
                        log::debug!("Allowing through call to bind with address family af_inet6");
                        Ok((MockAction::DontMock, None))
                    }
                } else {
                    log::debug!("Mocking bind call with EINVAL as it's not supported");
                    regs.rax = -EINVAL as u64;
                    Ok((MockAction::NoOp(regs), None))
                }
            }
            None => {
                log::debug!("Mocking bind call with EINVAL as it's not supported");
                regs.rax = -EINVAL as u64;
                Ok((MockAction::NoOp(regs), None))
            }
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        match *self {
            Mock { .. } => Ok(()),
        }
    }
}
