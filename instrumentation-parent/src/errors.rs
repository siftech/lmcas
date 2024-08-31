// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! Error types that need to be specially recognized.

use std::{
    error::Error,
    fmt::{Display, Formatter, Result as FmtResult},
};

#[derive(Debug)]
pub struct UnhandledSyscall(pub u64);

impl Display for UnhandledSyscall {
    fn fmt(&self, fmt: &mut Formatter) -> FmtResult {
        write!(fmt, "Unknown syscall number {} (0x{:x})\nFor a list of syscalls and their corresponding symbolic names, see https://github.com/torvalds/linux/blob/master/arch/x86/entry/syscalls/syscall_64.tbl\n", self.0, self.0)
    }
}

impl Error for UnhandledSyscall {}
