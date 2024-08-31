// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! This module and its submodules encode the knowledge we have about syscalls.
//!
//! Each submodule that starts with `sys_` represents one syscall. They should define three types,
//! `Args`, `Mock`, and `Output`. The `make_args!`, `make_empty_mock!`, and `make_output!` macros
//! may be useful for defining these. `Args` stores the arguments to the syscall, and should impl
//! the `SyscallArgs` trait. `Output` stores any information about what it did that we need for the
//! syscall shims in the final binary to work, and should impl the `SyscallOutput` trait. `Mock`
//! stores a configuration option that can be used to specify behavioral changes for the syscall
//! (e.g. redirecting files, making a syscall always return the same value, etc.). The submodules
//! should also define a `const NUM: u64`, which is the syscall number.
//!
//! A macro (`make_types!`) is then used to define the `KnownSyscallArgs`, `KnownSyscall`, and
//! `SyscallMocks` types.

mod macros;
pub mod structures;

use anyhow::{Context, Result};
use bstr::BString;
use nix::{
    libc::{iovec, user_regs_struct},
    unistd::Pid,
};
use serde::{Deserialize, Serialize};
use std::{collections::HashMap, fmt::Debug, path::PathBuf};

use crate::U64AsString;

macros::make_types! {
    sys_read,
    sys_write,
    sys_open,
    sys_stat,
    sys_fstat,
    sys_close,
    sys_lseek,
    sys_mmap,
    sys_mprotect,
    sys_munmap,
    sys_brk,
    sys_rt_sigaction,
    sys_rt_sigprocmask,
    sys_ioctl,
    sys_pread,
    sys_readv,
    sys_writev,
    sys_pipe,
    sys_getpid,
    sys_socket,
    sys_connect,
    sys_bind,
    sys_listen,
    sys_setsockopt,
    sys_uname,
    sys_openat,
    sys_fcntl,
    sys_mkdir,
    sys_getuid,
    sys_getgid,
    sys_geteuid,
    sys_getppid,
    sys_getgroups,
    sys_sched_getaffinity,
    sys_clock_gettime,
    sys_clock_getres,
    sys_prlimit,
    sys_epoll_create1,
    sys_memfd_create,
    sys_umask,
}

pub trait SyscallArgs: Copy + Debug + Serialize + Sized {
    fn from_regs(regs: user_regs_struct) -> Result<Self>;
}

pub trait SyscallOutput<Args: SyscallArgs>: Debug + Serialize + Sized {
    fn from_regs(
        function_pointer_table: &HashMap<u64, u64>,
        pid: Pid,
        regs: user_regs_struct,
        args: Args,
    ) -> Result<Self>;
}

/// A type for vectored IO, containing the address and length of the buffer, as well as its
/// contents after the syscall completes.
#[derive(Clone, Debug, Default, Serialize, Deserialize, PartialEq)]
pub struct IoVec {
    pub base: U64AsString,
    pub len: U64AsString,
    pub data: BString,
}

impl IoVec {
    pub fn from_iovec_and_bytes(iov: iovec, bytes: Vec<u8>) -> IoVec {
        IoVec {
            base: (iov.iov_base as u64).into(),
            len: (iov.iov_len as u64).into(),
            data: BString::from(bytes),
        }
    }
}

/// The action to take when mocking a syscall.
#[derive(Debug)]
pub enum MockAction {
    /// Allow the process to perform the syscall it intended to.
    DontMock,

    /// Don't perform any syscall. Instead, set the registers to the given ones.
    NoOp(user_regs_struct),

    /// Instead of performing the syscall the child process intended, perform the one indicated by
    /// the given register state.
    Replace(user_regs_struct),
}
