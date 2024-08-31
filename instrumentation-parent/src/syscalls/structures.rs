// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! These structures correspond to ones defined in nix::libc; however, these ones implement
//! Serialize, as well as our own Pod trait.

use crate::{pod::Pod, I64AsString, U64AsString};
use serde::{Deserialize, Serialize};

#[repr(C, packed)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct stat {
    pub st_dev: U64AsString,
    pub st_ino: U64AsString,
    pub st_nlink: U64AsString,
    pub st_mode: u32,
    pub st_uid: u32,
    pub st_gid: u32,
    __pad0: i32,
    pub st_rdev: U64AsString,
    pub st_size: I64AsString,
    pub st_blksize: I64AsString,
    pub st_blocks: I64AsString,
    pub st_atime: I64AsString,
    pub st_atime_nsec: I64AsString,
    pub st_mtime: I64AsString,
    pub st_mtime_nsec: I64AsString,
    pub st_ctime: I64AsString,
    pub st_ctime_nsec: I64AsString,
    __unused: [I64AsString; 3],
}

unsafe impl Pod for stat {}

impl crate::FuzzyEq<stat> for stat {
    fn fuzzy_eq(&self, other: &stat) -> Option<()> {
        let st_dev_left = self.st_dev;
        let st_dev_right = other.st_dev;

        let st_ino_left = self.st_ino;
        let st_ino_right = other.st_ino;

        let st_nlink_left = self.st_nlink;
        let st_nlink_right = other.st_nlink;

        let st_size_left = self.st_size;
        let st_size_right = other.st_size;

        let st_blksize_left = self.st_blksize;
        let st_blksize_right = other.st_blksize;

        let st_blocks_left = self.st_blocks;
        let st_blocks_right = other.st_blocks;

        (st_dev_left == st_dev_right
            && st_ino_left == st_ino_right
            && st_nlink_left == st_nlink_right
            && self.st_mode == other.st_mode
            && self.st_uid == other.st_uid
            && self.st_gid == other.st_gid
            && st_size_left == st_size_right
            && st_blksize_left == st_blksize_right
            && st_blocks_left == st_blocks_right)
            .then_some(())
    }
}

#[repr(C, packed)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct timespec {
    pub tv_sec: I64AsString,
    pub tv_nsec: I64AsString,
}

unsafe impl Pod for timespec {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct sigset_t {
    // According to https://elixir.bootlin.com/linux/latest/source/arch/x86/include/asm/signal.h#L24
    __val: [U64AsString; 1],
}

unsafe impl Pod for sigset_t {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct sigaction {
    pub sa_handler: U64AsString,
    pub sa_flags: U64AsString,
    pub sa_restorer: U64AsString,
    pub sa_mask: sigset_t,
}

unsafe impl Pod for sigaction {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct rlimit {
    pub rlim_cur: U64AsString,
    pub rlim_max: U64AsString,
}

unsafe impl Pod for rlimit {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct sockaddr {
    pub sa_family: u16,
    pub sa_data: [i8; 14],
}

unsafe impl Pod for sockaddr {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct in_addr {
    pub s_addr: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct sockaddr_in {
    pub sin_family: u16,
    pub sin_port: u16,
    pub sin_addr: in_addr,
    pub sin_zero: [u8; 8],
}

unsafe impl Pod for sockaddr_in {}
unsafe impl Pod for in_addr {}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
#[repr(align(4))]
pub struct in6_addr {
    pub s6_addr: [u8; 16],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct sockaddr_in6 {
    pub sin6_family: u16,
    pub sin6_port: u16,
    pub sin6_flowinfo: u32,
    pub sin6_addr: in6_addr,
    pub sin6_scope_id: u32,
}

unsafe impl Pod for sockaddr_in6 {}
unsafe impl Pod for in6_addr {}

unsafe impl<const N: usize> Pod for [u8; N] {}
