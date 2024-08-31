// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use crate::U64AsString;

use std::{collections::HashMap, path::Path};

use nix::{
    libc::{user_regs_struct, SIGHUP},
    unistd::Pid,
};
use serde::Deserialize;

use anyhow::{bail, Context, Result};

use crate::{pod::write_bytes, TapeEntry};

use super::{
    macros::{impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_output},
    KnownSyscallArgs, MockAction,
};

pub const NUM: u64 = nix::libc::SYS_rt_sigaction as u64;

make_args! {
    sig: i32,
    act_ptr: U64AsString,
    oact_ptr: U64AsString,
    sigsetsize: U64AsString,
}

// We ignore act_ptr and oact_ptr here because the memory address can vary
// despite having equivalent semantics
impl_fuzzyeq_partial_match!(Args, sig, sigsetsize);

make_output! {
    return_code = return,
    sighandler = sighandler_annot,
    act = optional_pod(act_ptr, super::structures::sigaction),
    oact = optional_pod(oact_ptr, super::structures::sigaction),
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields, rename_all = "snake_case")]
pub struct Mock {
    replace_sighup: bool,
}

impl Mock {
    pub fn check_syscall_for_mocking(
        mock: Option<&Mock>,
        _parent_page_addr: u64,
        _function_pointer_table: &HashMap<u64, u64>,
        noop_sighandler_addr: u64,
        pid: Pid,
        regs: user_regs_struct,
        args: &Args,
    ) -> Result<(MockAction, Option<TapeEntry<KnownSyscallArgs>>)> {
        match mock {
            Some(Mock { replace_sighup }) => {
                if *replace_sighup && args.sig == SIGHUP {
                    log::debug!("Mocking call to sigaction on sighup with noop function.");
                    log::debug!(
                        "Writing address {} to child's memory.",
                        noop_sighandler_addr
                    );
                    write_bytes(pid, regs.rsi, &noop_sighandler_addr.to_le_bytes())
                        .context("Failed to write noop sighandler address to child's memory")?;
                    Ok((MockAction::Replace(regs), None))
                } else {
                    Ok((MockAction::DontMock, None))
                }
            }
            None => {
                if args.sig == SIGHUP {
                    bail!("I've noticed you haven't provided a specification for replacing sighup in the sys_rt_sigaction syscall. \
                           Some programs use SIGHUP to call a handler that reloads the configuration, which could undo \
                           the specialization or cause unintended behavior. You should set a specification for \
                           the sys_rt_sigaction syscall, an example that sets the signal handler to a noop is below.\n\
                           ```\n\
                            \"sys_rt_sigaction\": {{\n\
                                \"replace_sighup\" : true\n\
                            }},\n\
                           ```\n\
                           You could also set the above \"replace_sighup\" property to false to allow the sys_rt_sigaction syscall \n\
                           for SIGHUP through without any changes, and stop this message from appearing.
                           ")
                } else {
                    Ok((MockAction::DontMock, None))
                }
            }
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        match *self {
            Mock { .. } => Ok(()),
        }
    }
}
