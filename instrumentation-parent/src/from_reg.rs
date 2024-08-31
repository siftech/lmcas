// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

use anyhow::Result;
use std::convert::TryInto;

use crate::{I64AsString, U64AsString};

/// A trait for types that can be converted from a register's value.
pub trait FromRegister: Sized {
    fn from_reg(reg: u64) -> Result<Self>;
}

impl FromRegister for i32 {
    fn from_reg(reg: u64) -> Result<i32> {
        Ok(i64::from_reg(reg)?.try_into()?)
    }
}

impl FromRegister for u32 {
    fn from_reg(reg: u64) -> Result<u32> {
        Ok(u64::from_reg(reg)?.try_into()?)
    }
}

impl FromRegister for i64 {
    fn from_reg(reg: u64) -> Result<i64> {
        Ok(reg as i64)
    }
}

impl FromRegister for I64AsString {
    fn from_reg(reg: u64) -> Result<I64AsString> {
        Ok((reg as i64).into())
    }
}

impl FromRegister for u64 {
    fn from_reg(reg: u64) -> Result<u64> {
        Ok(reg)
    }
}

impl FromRegister for U64AsString {
    fn from_reg(reg: u64) -> Result<U64AsString> {
        Ok(U64AsString(reg))
    }
}
