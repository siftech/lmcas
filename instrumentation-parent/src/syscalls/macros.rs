// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

macro_rules! make_types {
    ($($name:ident),* $(,)*) => {
        $(pub mod $name;)*

        /// The arguments to a syscall we have a shim for.
        #[allow(non_camel_case_types)]
        #[derive(Clone, Copy, Debug, Deserialize, Serialize)]
        pub enum KnownSyscallArgs {
            $($name { args: $name::Args }),*
        }

        impl $crate::syscalls::SyscallArgs for KnownSyscallArgs {
            fn from_regs(regs: user_regs_struct) -> anyhow::Result<KnownSyscallArgs> {
                match regs.orig_rax {
                    $($name::NUM => {
                        Ok(KnownSyscallArgs::$name {
                            args: anyhow::Context::context(
                                $name::Args::from_regs(regs),
                                concat!("Failed to decode syscall args for ", stringify!($name)),
                            )?
                        })
                    },)*
                    _ => Err($crate::errors::UnhandledSyscall(regs.orig_rax).into()),
                }
            }
        }

        /// The arguments to and output from a syscall we have a shim for.
        #[allow(non_camel_case_types)]
        #[derive(Debug, Deserialize, Serialize)]
        #[serde(tag = "syscall")]
        pub enum KnownSyscall {
            $($name {
                #[serde(flatten)]
                args: $name::Args,
                #[serde(flatten)]
                output: $name::Output,
            }),*
        }

        impl $crate::FuzzyEq for KnownSyscall {
            fn fuzzy_eq(&self, other: &KnownSyscall) -> Option<()> {
                match (self, other) {
                    $((KnownSyscall::$name {args: args_left, output: output_left},
                       KnownSyscall::$name {args: args_right, output: output_right}) => {
                        args_left.fuzzy_eq(args_right)?;
                        output_left.fuzzy_eq(output_right)
                    }),*
                    // At this point, only comparisons of two different syscalls should be appearing.
                    (_,_) => None
                }
            }
        }


        impl $crate::syscalls::SyscallOutput<KnownSyscallArgs> for KnownSyscall {
            fn from_regs(
                function_pointer_table: &std::collections::HashMap<u64, u64>,
                pid: Pid,
                regs: user_regs_struct,
                args: KnownSyscallArgs,
            ) -> anyhow::Result<KnownSyscall> {
                match args {
                    $(KnownSyscallArgs::$name { args } => {
                        let output = anyhow::Context::with_context(
                            $name::Output::from_regs(function_pointer_table, pid, regs, args),
                            || format!(
                                "Failed to decode syscall output for {} with args: {:#?}",
                                stringify!($name),
                                args,
                            ),
                        )?;
                        Ok(KnownSyscall::$name { args, output })
                    }),*
                }
            }
        }



        /// Any customizations that could be applied in order to mock the syscall out.
        #[derive(Debug, Default, Deserialize)]
        #[serde(default, deny_unknown_fields)]
        pub struct SyscallMocks {
            $($name : Option<$name::Mock>,)*

            /// The directory relative paths are relative to.
            relative_path_dir: Option<PathBuf>,
        }

        impl SyscallMocks {
            pub fn check_syscall_for_mocking(
                &self,
                parent_page_addr: u64,
                function_pointer_table: &std::collections::HashMap<u64, u64>,
                noop_sighandler_addr: u64,
                pid: Pid,
                regs: user_regs_struct,
                args: &KnownSyscallArgs,
            ) -> Result<(crate::syscalls::MockAction, Option<crate::TapeEntry<crate::syscalls::KnownSyscallArgs>>)> {
                match args {
                    $(KnownSyscallArgs::$name { args } => {
                        $name::Mock::check_syscall_for_mocking(
                            self.$name.as_ref(),
                            parent_page_addr,
                            function_pointer_table,
                            noop_sighandler_addr,
                            pid,
                            regs,
                            args,
                        ).with_context(|| format!(
                            concat!("Failed to mock ", stringify!($name), " with {:?}"),
                            self.$name.as_ref(),
                        ))
                    }),*
                }
            }

            pub fn setup_and_validate(&mut self) -> Result<()> {
                let relative_path_dir = match &self.relative_path_dir {
                    Some(relative_path_dir) => std::fs::canonicalize(relative_path_dir)
                        .context("Failed to normalize relative_path_dir")?,
                    None => std::env::current_dir()
                        .context("Failed to get current working directory")?
                };

                $(if let Some(mock) = &mut self.$name {
                    mock.setup_and_validate(&relative_path_dir).context(
                        concat!("Failed to setup and validate ", stringify!($name)),
                    )?;
                })*

                Ok(())
            }
        }
    };
}

/// Defines an `Args` type for a syscall. See the `sys_write` module for an example.
macro_rules! make_args {
    ($($arg_name:ident : $arg_ty:ty),* $(,)*) => {
        #[derive(Clone, Copy, Debug, serde::Deserialize, serde::Serialize, PartialEq)]
        pub struct Args {
            $(pub $arg_name: $arg_ty),*
        }

        impl $crate::syscalls::SyscallArgs for Args {
            #[allow(unused_mut, unused_variables)]
            fn from_regs(regs: nix::libc::user_regs_struct) -> anyhow::Result<Args> {
                let mut out = Args { $($arg_name: Default::default()),* };
                $crate::syscalls::macros::make_args__from_regs__helper! {
                    regs, out,
                    ($($arg_name)*),
                    (rdi rsi rdx r10 r8 r9)
                }
                Ok(out)
            }
        }
    };
}

/// Defines an empty `Mock` type that has the required methods.
macro_rules! make_empty_mock {
    () => {
        pub type Mock = NoMock;

        #[derive(Debug, serde::Deserialize)]
        pub enum NoMock {}

        impl NoMock {
            pub fn check_syscall_for_mocking(
                mock: Option<&NoMock>,
                _parent_page_addr: u64,
                _function_pointer_table: &std::collections::HashMap<u64, u64>,
                _noop_sighandler_addr: u64,
                _pid: nix::unistd::Pid,
                _regs: nix::libc::user_regs_struct,
                _args: &Args,
            ) -> anyhow::Result<(
                crate::syscalls::MockAction,
                Option<crate::TapeEntry<crate::syscalls::KnownSyscallArgs>>,
            )> {
                match mock {
                    Some(mock) => match *mock {},
                    None => Ok((crate::syscalls::MockAction::DontMock, None)),
                }
            }

            pub fn setup_and_validate(
                &mut self,
                _relative_path_dir: &std::path::Path,
            ) -> anyhow::Result<()> {
                match *self {}
            }
        }
    };
}

/// Defines a simple mock type for mocking just the return value of a syscall.
/// It creates a struct for mocking just that name given the name you provide.
macro_rules! make_return_value_mock {
    ($syscall_name:ident) => {
        #[derive(Debug, serde::Deserialize)]
        #[serde(deny_unknown_fields)]
        pub struct Mock {
            $syscall_name: u32,
        }

        impl Mock {
            pub fn check_syscall_for_mocking(
                mock: Option<&Mock>,
                _parent_page_addr: u64,
                _function_pointer_table: &std::collections::HashMap<u64, u64>,
                _noop_sighandler_addr: u64,
                _pid: nix::unistd::Pid,
                mut regs: nix::libc::user_regs_struct,
                _args: &Args,
            ) -> anyhow::Result<(crate::syscalls::MockAction, Option<crate::TapeEntry<crate::syscalls::KnownSyscallArgs>>)> {
                match mock {
                    Some(&Mock { $syscall_name }) => {
                        regs.rax = $syscall_name as u64;
                        log::debug!("replacing call to sys_get{} with return {:?}", stringify!($syscall_name), $syscall_name);
                        Ok((crate::syscalls::MockAction::NoOp(regs), None))
                    }
                    None => Ok((crate::syscalls::MockAction::DontMock, None)),
                }
            }

            pub fn setup_and_validate(&mut self, _relative_path_dir: &std::path::Path) -> anyhow::Result<()> {
                match *self {
                    Mock { .. } => Ok(()),
                }
            }
        }
    };
}

/// Defines a FuzzyEq impl that just falls back to PartialEq.
macro_rules! impl_fuzzyeq_exact_match {
    ($type:ident) => {
        impl crate::FuzzyEq<$type> for $type {
            fn fuzzy_eq(&self, other: &$type) -> Option<()> {
                self.eq(other).then_some(())
            }
        }
    };
}

/// Defines a FuzzyEq impl that compares with PartialEq the fields you specify.
macro_rules! impl_fuzzyeq_partial_match{
    ($type:ident) => {
        //If you provide zero options, it just returns true
        //This can occur for syscall args like sys_brk, which has _just_
        //an address for an argument.
        impl crate::FuzzyEq<$type> for $type {
            fn fuzzy_eq(&self, _: &$type) -> Option<()> {
               Some(())
            }
        }
    };
    ($type:ident, $($field:ident),* $(,)?) => {
        impl crate::FuzzyEq<$type> for $type {
            fn fuzzy_eq(&self, other: &$type) -> Option<()> {
                ($(self.$field == other.$field)&&*).then_some(())
            }
        }
    };
}

// This helper is used to map arguments to registers.
macro_rules! make_args__from_regs__helper {
    ($regs:ident, $out:ident, (), ($($_:tt)*)) => {};
    (
        $regs:ident,
        $out:ident,
        ($hd_arg:ident $($tl_args:ident)*),
        ($hd_reg:ident $($tl_regs:ident)*)
    ) => {{
        $out.$hd_arg = anyhow::Context::with_context(
            $crate::from_reg::FromRegister::from_reg($regs.$hd_reg),
            || format!(
                "Failed to read field {} from register {} with value 0x{:016x}",
                stringify!($hd_arg),
                stringify!($hd_reg),
                $regs.$hd_reg,
            ),
        )?;
        $crate::syscalls::macros::make_args__from_regs__helper! {
            $regs,
            $out,
            ($($tl_args)*),
            ($($tl_regs)*)
        }
    }};
    ($regs:ident, $out:ident, ($hd_arg:ident $($tl_args:ident)*), ()) => {
        compile_error!(concat!(
            "Out of registers for argument ",
            stringify!($hd_arg),
        ));
    };
}

/// Defines an `Output` type for a syscall. The arguments to this macro are effectively
/// `$($field_name:ident = $field_specifier:expr),* $(,)*`. Fields are defined by a
/// *field specifier*, which provides both the type and the way it's loaded from the child process.
///
/// Field specifiers are as follows:
///
/// - `return`: The return code of the syscall, as a `u64`.
/// - `sighandler_annot`: The annotation id of the sighandler function, which is read from the address that is pointed to by
///   the RSI register. It's read as a `u64`.
/// - `bytes($ptr:ident, $len:ident)`: Reads a `BString` out of the child process, starting at the
///   field of the arguments struct named `$ptr`. The length is the field of the arguments struct
///   named `$len`.
/// - `bytes($ptr:ident, $len:literal)`: Reads a `BString` out of the child process, starting at the
///   field of the arguments struct named `$ptr`. The length is `$len`.
/// - `pod($ptr:ident, $ty:ty)`: Reads a `Pod` type from the child process, from the field of the
///   arguments struct named `$ptr`.
/// - `optional_pod($ptr:ident, $ty:ty)`: If `$ptr` is non-null, reads a `Pod` type from the child
///   process, from the field of the arguments struct named `$ptr`, returning it wrapped in Some.
///   If `$ptr` is 0, returns None.
/// - `pods($ptr:ident, $ty:ty, $num:ident)`: Reads `$num` `Pod` type elements from the child
///   process, from the field of the arguments struct named `$ptr`.
/// - `pods($ptr:ident, $ty:ty, $num:literal)`: Reads `$num` `Pod` type elements from the child
///   process, from the field of the arguments struct named `$ptr`.
/// - `string($ptr:ident)`: Reads a `CString` out of the child process, starting at the field of
///   the arguments struct named `$ptr`.
/// - `iovs($iov:ident, $iovcnt:ident)`: Reads a `Vec<IoVec>` out of the child process, containing
///   both the bounds of the buffers and their contents.
macro_rules! make_output {
    ($($tokens:tt)*) => {
        $crate::syscalls::macros::make_output__parser!([], [$($tokens)*]);
    }
}

// The helper macro that handles the output of the `make_output!` macro.
macro_rules! make_output__helper {
    ($(($field_name:ident, $kind:ident($($args:tt),*))),*) => {
        #[derive(Clone, Debug, serde::Serialize, serde::Deserialize, PartialEq)]
        pub struct Output {
            $($field_name: $crate::syscalls::macros::make_output__type_of_field_specifier!(
                $kind($($args),*)
            )),*
        }

        impl $crate::syscalls::SyscallOutput<Args> for Output {
            fn from_regs(
                function_pointer_table: &std::collections::HashMap<u64, u64>,
                pid: nix::unistd::Pid,
                regs: nix::libc::user_regs_struct,
                args: Args,
            ) -> anyhow::Result<Output> {
                // Treat the arguments as used, preventing an unused-variables warning.
                let _ = (function_pointer_table, pid, regs, args);

                Ok(Output {
                    $($field_name: $crate::syscalls::macros::make_output__field_from_regs!(
                        function_pointer_table,
                        pid,
                        regs,
                        args,
                        $kind($($args),*)
                    )),*
                })
            }
        }
    };
}

/// The parser for the `make_output!` macro. Takes the contents of the `make_output!` (in its second
/// list of tokens), and parses it into a series of pairs of `(field_name,
/// parsed_field_specifier)`, which are stored in the first list of tokens as it recurses.
///
/// Parsed field specifiers are as follows:
///
/// - `return()`: The equivalent of the `return` field specifier.
/// - `sighandler_annot()`: The equivalent of the `sighandler_annot` field specifier.
/// - `bytes_aa($ptr:ident, $len:ident)`: The equivalent of the `bytes` field specifier when given
///   two identifiers.
/// - `bytes_ac($ptr:ident, $len:literal)`: The equivalent of the `bytes` field specifier when given
///   an identifier and a literal.
/// - `iovs_aa($iov:ident, $iovcnt:ident)`: The equivalent of the `iovs` field specifier when given
///   two identifiers.
/// - `pod_a($ptr:ident, $ty:ty)`: The equivalent of the `pod` field specifier when given an
///   identifier.
/// - `optional_pod_a($ptr:ident, $ty:ty)`: The equivalent of the `pod` field specifier when given an
///   identifier, but can return None if ptr is null.
/// - `pods_aa($ptr:ident, $ty:ty, $num:ident)`: The equivalent of the `pods` field specifier when
///   given two identifiers.
/// - `pods_ac($ptr:ident, $ty:ty, $num:literal)`: The equivalent of the `pods` field specifier
///   when given an identifier and a literal.
/// - `string($ptr:ident)`: The equivalent of the `string` field specifier when given an
///   identifier.
macro_rules! make_output__parser {
    // The base case; we continue on to make_output__helper.
    ([$($parsed:tt),*], []) => {
        $crate::syscalls::macros::make_output__helper!($($parsed),*);
    };

    // Skip any extraneous commas.
    ([$($parsed:tt),*], [, $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([$($parsed),*], [$($rest)*]);
    };

    // Parse the return field specifier.
    ([$($parsed:tt),*], [$field_name:ident = return $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, return())
        ], [
            $($rest)*
        ]);
    };

    // Parse the signal handler annotation field specifier.
    ([$($parsed:tt),*], [$field_name:ident = sighandler_annot $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, sighandler_annot())
        ], [
            $($rest)*
        ]);
    };

    // Parse the bytes field specifier when it takes two identifiers (corresponding to field names
    // from the args struct).
    ([$($parsed:tt),*], [$field_name:ident = bytes($ptr:ident, $len:ident) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, bytes_aa($ptr, $len))
        ], [
            $($rest)*
        ]);
    };

    // Parse the bytes field specifier when it takes an identifier and a
    // literal, where the identifier is from the arguments struct.
    ([$($parsed:tt),*], [$field_name:ident = bytes($ptr:ident, $len:literal) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, bytes_ac($ptr, $len))
        ], [
            $($rest)*
        ]);
    };

    ([$($parsed:tt),*], [$field_name:ident = pod($ptr:ident, $ty:ty) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, pod_a($ptr, $ty))
        ], [
            $($rest)*
        ]);
    };
    ([$($parsed:tt),*], [$field_name:ident = optional_pod($ptr:ident, $ty:ty) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, optional_pod_a($ptr, $ty))
        ], [
            $($rest)*
        ]);
    };
    ([$($parsed:tt),*], [$field_name:ident = pods($ptr:ident, $ty:ty, $num:ident) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, pods_aa($ptr, $ty, $num))
        ], [
            $($rest)*
        ]);
    };
    ([$($parsed:tt),*], [$field_name:ident = pods($ptr:ident, $ty:ty, $num:literal) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, pods_ac($ptr, $ty, $num))
        ], [
            $($rest)*
        ]);
    };

    // Parse the iovs field specifier when it takes two identifiers.
    ([$($parsed:tt),*], [$field_name:ident = iovs($ptr:ident, $len:ident) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, iovs_aa($ptr, $len))
        ], [
            $($rest)*
        ]);
    };

    // Parse the string field specifier when it takes an identifier.
    ([$($parsed:tt),*], [$field_name:ident = string($ptr:ident) $($rest:tt)*]) => {
        $crate::syscalls::macros::make_output__parser!([
            $($parsed,)*
            ($field_name, string_a($ptr))
        ], [
            $($rest)*
        ]);
    };
}

/// Given a parsed field specifier, returns the type the corresponding field on the output struct
/// should have.
macro_rules! make_output__type_of_field_specifier {
    (return()) => { crate::U64AsString };
    (sighandler_annot()) => { crate::U64AsString };
    (bytes_aa($ptr:ident, $len:ident)) => { bstr::BString };
    (bytes_ac($ptr:ident, $len:literal)) => { bstr::BString };
    (iovs_aa($ptr:ident, $len:ident)) => { Vec<$crate::syscalls::IoVec> };
    (pod_a($ptr:ident, $ty:ty)) => { $ty };
    (optional_pod_a($ptr:ident, $ty:ty)) => { Option<$ty> };
    (pods_aa($ptr:ident, $ty:ty, $num:ident)) => { Vec<$ty> };
    (pods_ac($ptr:ident, $ty:ty, $num:literal)) => { Vec<$ty> };
    (string_a($ptr:ident)) => { std::ffi::CString };
}

/// Returns an expression (which may perform control-flow) to get the value of the field from the
/// child process, its registers, and the syscall arguments.
macro_rules! make_output__field_from_regs {
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, return()) => {
        crate::U64AsString($regs.rax)
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, sighandler_annot()) => {
        // SIG_IGN, SIG_DFL values can be 0 or 1.
        if $regs.rsi < 1 {
            U64AsString::from($regs.rsi)
        // All others are function pointers.
        } else {
            U64AsString::from(
                *($function_pointer_table
                    .get(&u64::from_le_bytes($crate::pod::read_pod($pid, $regs.rsi)?))
                    .unwrap_or(&u64::from_le_bytes($crate::pod::read_pod($pid, $regs.rsi)?))),
            )
        }
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, bytes_aa($ptr:ident, $len:ident)) => {
        bstr::BString::from($crate::pod::read_bytes($pid, $args.$ptr.0, $args.$len.0)?)
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, bytes_ac($ptr:ident, $len:literal)) => {
        bstr::BString::from($crate::pod::read_bytes($pid, $args.$ptr.0, $len)?)
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, iovs_aa($ptr:ident, $len:ident)) => {
        $crate::pod::read_iovs($pid, $args.$ptr.0, $args.$len.0)?
            .into_iter()
            .map(|(iov, bytes)| $crate::syscalls::IoVec::from_iovec_and_bytes(iov, bytes))
            .collect()
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, pod_a($ptr:ident, $ty:ty)) => {
        $crate::pod::read_pod($pid, $args.$ptr.0)?
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, optional_pod_a($ptr:ident, $ty:ty)) => {
        $crate::pod::read_pod_option($pid, $args.$ptr.0)
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, pods_aa($ptr:ident, $ty:ty, $num:ident)) => {
        $crate::pod::read_pods($pid, $args.$ptr.into())
            .take($args.$num as usize)
            .collect::<Result<Vec<$ty>, anyhow::Error>>()?
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, pods_ac($ptr:ident, $ty:ty, $num:literal)) => {
        $crate::pod::read_pods($pid, $args.$ptr.into())
            .take($num as usize)
            .collect::<Result<Vec<$ty>, anyhow::Error>>()?
    };
    ($function_pointer_table:expr, $pid:expr, $regs:expr, $args:expr, string_a($ptr:ident)) => {
        $crate::pod::read_cstring($pid, $args.$ptr.into())?
    };
}

pub(crate) use {
    impl_fuzzyeq_exact_match, impl_fuzzyeq_partial_match, make_args, make_args__from_regs__helper,
    make_return_value_mock, make_empty_mock, make_output, make_output__field_from_regs, make_output__helper,
    make_output__parser, make_output__type_of_field_specifier, make_types,
};
