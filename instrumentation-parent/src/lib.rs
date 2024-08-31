// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

pub mod errors;
mod from_reg;
pub mod pod;
pub mod syscalls;

use serde::{
    de::{self, Deserializer, Visitor},
    Deserialize, Serialize, Serializer,
};
use std::{collections::HashMap, fmt, path::PathBuf};
use syscalls::SyscallMocks;

use anyhow::Result;

// This is generic so that we can put the syscall information in after getting the results of the
// syscall as performed by the process.
#[derive(Debug, Serialize, Deserialize)]
#[serde(rename_all = "snake_case", tag = "type")]
pub enum TapeEntry<SyscallData> {
    BasicBlockStart { basic_block_id: U64AsString },
    CallInfo { start: bool },
    SyscallStart(SyscallData),
    Ret,
    CondBr { taken: bool },
    Switch { value: U64AsString },
    IndirectBr { addr: U64AsString },
    Unreachable,
}

impl<SyscallData> TapeEntry<SyscallData> {
    pub fn map_syscall_start_result<NewSyscallData>(
        self,
        func: impl FnOnce(SyscallData) -> Result<NewSyscallData>,
    ) -> Result<TapeEntry<NewSyscallData>> {
        match self {
            TapeEntry::BasicBlockStart { basic_block_id } => {
                Ok(TapeEntry::BasicBlockStart { basic_block_id })
            }
            TapeEntry::CallInfo { start } => Ok(TapeEntry::CallInfo { start }),
            TapeEntry::SyscallStart(data) => Ok(TapeEntry::SyscallStart(func(data)?)),
            TapeEntry::Ret => Ok(TapeEntry::Ret),
            TapeEntry::CondBr { taken } => Ok(TapeEntry::CondBr { taken }),
            TapeEntry::Switch { value } => Ok(TapeEntry::Switch { value }),
            TapeEntry::IndirectBr { addr } => Ok(TapeEntry::IndirectBr { addr }),
            TapeEntry::Unreachable => Ok(TapeEntry::Unreachable),
        }
    }
}

#[derive(Debug, Deserialize)]
pub struct InstrumentationSpec {
    /// The binary the instrumentation-parent should run. This must be an absolute path.
    pub binary: PathBuf,

    /// The arguments the binary should be run with. These *include* `argv[0]`!
    pub args: Vec<String>,

    /// The environment variables the binary should be run with. The runtime does not automatically
    /// include "commonly used" environment variables like `PWD` or `USER`; these need to be
    /// specified if they are required.
    pub env: HashMap<String, String>,

    /// The directory the binary should be run in. This must be an absolute path.
    pub cwd: PathBuf,

    /// A list of indexes to ignore when using the robustness checker to look for sources of nondeterministic behavior.
    pub ignore_indexes: Option<Vec<usize>>,

    /// Any mocks to apply to the syscalls that are instrumented.
    #[serde(default)]
    pub syscall_mocks: SyscallMocks,
}

#[derive(Debug, Copy, Clone, Default, PartialEq)]
pub struct U64AsString(pub u64);

impl From<u64> for U64AsString {
    fn from(value: u64) -> Self {
        U64AsString(value)
    }
}

impl From<U64AsString> for u64 {
    fn from(value: U64AsString) -> u64 {
        value.0
    }
}

impl<'de> Deserialize<'de> for U64AsString {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct IdVisitor;

        impl<'de> Visitor<'de> for IdVisitor {
            type Value = U64AsString;

            fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
                f.write_str("U64 as a number or string")
            }

            fn visit_u64<E>(self, id: u64) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                Ok(U64AsString(id))
            }

            fn visit_str<E>(self, id: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                id.parse().map(U64AsString).map_err(de::Error::custom)
            }
        }

        deserializer.deserialize_any(IdVisitor)
    }
}

impl Serialize for U64AsString {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.0.to_string())
    }
}

#[derive(Debug, Copy, Clone, Default, PartialEq)]
pub struct I64AsString(i64);

impl From<i64> for I64AsString {
    fn from(value: i64) -> Self {
        I64AsString(value)
    }
}

impl From<I64AsString> for i64 {
    fn from(value: I64AsString) -> i64 {
        value.0
    }
}

impl fmt::LowerHex for I64AsString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let val = self.0;

        fmt::LowerHex::fmt(&val, f) // delegate to i64's implementation
    }
}

impl<'de> Deserialize<'de> for I64AsString {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct IdVisitor;

        impl<'de> Visitor<'de> for IdVisitor {
            type Value = I64AsString;

            fn expecting(&self, f: &mut fmt::Formatter) -> fmt::Result {
                f.write_str("I64 as a number or string")
            }

            fn visit_i64<E>(self, id: i64) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                Ok(I64AsString(id))
            }

            fn visit_str<E>(self, id: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                id.parse().map(I64AsString).map_err(de::Error::custom)
            }
        }

        deserializer.deserialize_any(IdVisitor)
    }
}

impl Serialize for I64AsString {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.0.to_string())
    }
}

/// A trait for comparisons of TapeEntries.
/// The "fuzzy" part is in reference to it being a comparison
/// that respects elements in tapes that might be different between
/// different executions but don't cause problems
/// (e.g memory addresses can change with aslr, kernel selecting address)
/// We interpret Some(()) as true, None as false.
pub trait FuzzyEq<Rhs = Self> {
    fn fuzzy_eq(&self, other: &Rhs) -> Option<()>;
}

impl<T: FuzzyEq> FuzzyEq for TapeEntry<T> {
    fn fuzzy_eq(&self, other: &TapeEntry<T>) -> Option<()> {
        match (self, other) {
            (
                TapeEntry::BasicBlockStart {
                    basic_block_id: bbid_left,
                },
                TapeEntry::BasicBlockStart {
                    basic_block_id: bbid_right,
                },
            ) => (bbid_left.0 == bbid_right.0).then_some(()),
            (
                TapeEntry::CallInfo { start: start_left },
                TapeEntry::CallInfo { start: start_right },
            ) => (start_left == start_right).then_some(()),
            (TapeEntry::SyscallStart(data_left), TapeEntry::SyscallStart(data_right)) => {
                data_left.fuzzy_eq(data_right)
            }
            (TapeEntry::Ret, TapeEntry::Ret) => Some(()),
            (TapeEntry::CondBr { taken: taken_left }, TapeEntry::CondBr { taken: taken_right }) => {
                (taken_left == taken_right).then_some(())
            }
            (TapeEntry::Switch { value: value_left }, TapeEntry::Switch { value: value_right }) => {
                (value_left == value_right).then_some(())
            }
            (
                TapeEntry::IndirectBr { addr: addr_left },
                TapeEntry::IndirectBr { addr: addr_right },
            ) => (addr_left == addr_right).then_some(()),
            (TapeEntry::Unreachable, TapeEntry::Unreachable) => Some(()),
            (_, _) => None,
        }
    }
}

impl<T: FuzzyEq> FuzzyEq for Vec<TapeEntry<T>> {
    fn fuzzy_eq(&self, other: &Self) -> Option<()> {
        if self.len() != other.len() {
            None
        } else {
            for (elem_left, elem_right) in self.iter().zip(other.iter()) {
                if elem_left.fuzzy_eq(elem_right).is_none() {
                    return None;
                }
            }
            Some(())
        }
    }
}
