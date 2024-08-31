// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! TODO: This needs a *BIG* rethink -- ioctls are so incredibly driver-specific that it seems
//! rather unreasonable to use a single generic handler for all of them.

use crate::{
    pod::write_bytes,
    syscalls::{
        macros::{impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_output},
        KnownSyscallArgs, MockAction,
    },
    TapeEntry, U64AsString,
};
use anyhow::{bail, Context, Result};
use nix::{
    libc::{user_regs_struct, SYS_ioctl, FIONBIO, TIOCGWINSZ},
    unistd::Pid,
};
use std::{collections::HashMap, path::Path};

pub const NUM: u64 = SYS_ioctl as u64;

make_args! {
    fd: i32,
    request: U64AsString,
    arg0: U64AsString,
    arg1: U64AsString,
    arg2: U64AsString,
    arg3: U64AsString,
}

// TODO: probably should branch on ioctl request number?
impl_fuzzyeq_partial_match!(Args, fd, request, arg0);

make_output! {
    return_code = return,
    arg0_contents = optional_pod(arg0, i32),
}

impl_fuzzyeq_exact_match!(Output);

#[derive(Debug, serde::Deserialize)]
pub struct Mock {
    #[serde(default)]
    terminal_dimensions: Option<WinSize>,

    /// Unsafe is in the name because the specialization-pass will not insert checking code for the
    /// return value or memory effects of this ioctl. Be very careful about removing that label!
    #[serde(default)]
    unsafe_allow_tiocgwinsz: bool,
}

#[derive(Debug, serde::Deserialize)]
pub struct WinSize {
    row: u16,
    col: u16,

    #[serde(default)]
    xpixel: u16,

    #[serde(default)]
    ypixel: u16,
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
        match (args.request.0, mock) {
            (FIONBIO, _) => {
                log::debug!(
                    "Allowing through call to sys_ioctl with FIONBIO on fd: {}",
                    args.fd
                );
                Ok((MockAction::DontMock, None))
            }

            (
                TIOCGWINSZ,
                Some(Mock {
                    terminal_dimensions: None,
                    unsafe_allow_tiocgwinsz: true,
                }),
            ) => Ok((MockAction::DontMock, None)),
            (
                TIOCGWINSZ,
                Some(Mock {
                    terminal_dimensions: Some(win_size),
                    unsafe_allow_tiocgwinsz: true,
                }),
            ) => {
                // TODO: Maybe this should use some write_pod thing?
                let bytes = [
                    win_size.row as u8,
                    (win_size.row >> 8) as u8,
                    win_size.col as u8,
                    (win_size.col >> 8) as u8,
                    win_size.xpixel as u8,
                    (win_size.xpixel >> 8) as u8,
                    win_size.ypixel as u8,
                    (win_size.ypixel >> 8) as u8,
                ];
                write_bytes(pid, args.arg0.0, &bytes)
                    .context("Failed to write mocked terminal size to child's memory")?;
                regs.rax = 0;
                Ok((MockAction::NoOp(regs), None))
            }

            _ => {
                bail!(
                "Rejecting ioctl call with request number: {} on fd: {}, as it's not supported.",
                args.request.0,
                args.fd
            )
            }
        }
    }

    pub fn setup_and_validate(&mut self, _relative_path_dir: &Path) -> Result<()> {
        Ok(())
    }
}
