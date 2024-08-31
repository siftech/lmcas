// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

mod proto;

use crate::proto::{read_ready_message, read_tape_entry_message};
use anyhow::{bail, ensure, Context, Result};
use clap::Parser;
use instrumentation_parent::{
    errors::UnhandledSyscall,
    syscalls::{
        KnownSyscall, KnownSyscallArgs, MockAction, SyscallArgs, SyscallMocks, SyscallOutput,
    },
    InstrumentationSpec, TapeEntry,
};
use nix::{
    libc::{user_regs_struct, SYS_write},
    sys::{
        ptrace,
        signal::Signal,
        wait::{waitpid, WaitStatus},
    },
    unistd::{close, dup2, pipe, Pid},
};
use std::{
    collections::HashMap,
    fs::File,
    io::{stdout, BufReader, BufWriter, Write},
    os::unix::prelude::{CommandExt, FromRawFd},
    path::PathBuf,
    process::{Child, Command, Stdio},
};
use stderrlog::StdErrLog;

#[derive(Debug, Parser)]
struct Args {
    /// Decreases the log level.
    #[clap(
        short,
        long,
        conflicts_with("verbose"),
        max_occurrences(2),
        parse(from_occurrences)
    )]
    quiet: usize,

    /// Increases the log level.
    #[clap(
        short,
        long,
        conflicts_with("quiet"),
        max_occurrences(3),
        parse(from_occurrences)
    )]
    verbose: usize,

    /// The spec file to use.
    instrumentation_spec_path: PathBuf,

    /// The file to output to. If not provided, will output to stdout.
    output_path: Option<PathBuf>,

    /// Don't fail with an error when an unhandled syscall occurs,
    #[clap(long)]
    no_fail_on_unhandled_syscall: bool,

    /// Replace sighup with a noop function.
    #[clap(long)]
    replace_sighup: bool,
}

fn main() -> Result<()> {
    // Get the command-line arguments.
    let args = Args::parse();

    // Set up logging.
    {
        let mut logger = StdErrLog::new();
        match args.quiet {
            0 => logger.verbosity(1 + args.verbose),
            1 => logger.verbosity(0),
            2 => logger.quiet(true),
            // UNREACHABLE: A maximum of two occurrences of quiet are allowed.
            _ => unreachable!(),
        };
        // UNWRAP: No other logger should be set up.
        logger.show_module_names(true).init().unwrap()
    }

    // Get the instrumentation specification from the command line arguments.
    let mut instrumentation_spec = {
        let file = File::open(&args.instrumentation_spec_path)
            .context("Failed to open instrumentation spec file")?;
        serde_json::from_reader::<_, InstrumentationSpec>(BufReader::new(file))
            .context("Failed to read instrumentation spec file")?
    };

    // Check that the instrumentation specification is valid.
    ensure!(
        instrumentation_spec.binary.is_absolute(),
        "binary must be an absolute path"
    );
    ensure!(
        !instrumentation_spec.args.is_empty(),
        "args must contain at least one entry (for argv[0])"
    );
    ensure!(
        instrumentation_spec.cwd.is_absolute(),
        "cwd must be an absolute path",
    );
    instrumentation_spec.syscall_mocks.setup_and_validate()?;

    // Create the pipe for communication with the child.
    let (pipe_read, pipe_write) = pipe().context("Failed to create pipe for IPC")?;

    // Construct the builder for the subprocess.
    let mut command = Command::new(&instrumentation_spec.binary);
    command
        .arg0(&instrumentation_spec.args[0])
        .args(&instrumentation_spec.args[1..])
        .env_clear()
        .envs(&instrumentation_spec.env)
        .current_dir(&instrumentation_spec.cwd)
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null());

    // We also need to register a closure to run before the execve() call in the subprocess (after
    // it gets fork()ed off), to ensure pipe_write is available at fd 1023, and to close pipe_read in
    // the child.
    //
    // UNSAFE: Our obligations are to not call async-signal-unsafe functions in the closure. Per
    // signal-safety(7), close() and dup2() are both async-signal-safe. ptrace(2) doesn't mention
    // signal safety, but this use is recommended in it, and it certainly shouldn't be trying to
    // mutate any process-global state.
    unsafe {
        command.pre_exec(move || {
            // We need to close the read side of the pipe, since it's not marked CLOEXEC and it's
            // not needed in the child.
            close(pipe_read)?;

            // If the write end of the pipe is already at fd 1023, we don't need to do anything else.
            if pipe_write != 1023 {
                // Then, we dup2 the write side of the pipe onto fd 1023.
                // - If fd 1023 was unallocated, this does the right thing.
                // - If another file was fd 1023, it was hopefully CLOEXEC (otherwise we'd be leaking
                //   it to the child), so it should be fine to dup2 over it.
                dup2(pipe_write, 1023)?;

                // Finally, we close the original fd to the write end of the pipe.
                close(pipe_write)?;
            }

            // Make ourselves available for ptracing.
            ptrace::traceme()?;

            Ok(())
        });
    }

    // The process is now fully set up, so we can now spawn it.
    log::debug!("About to spawn subprocess: {:#?}", command);
    let mut child = command.spawn().context("Failed to spawn child process")?;

    // Once the child has been spawned (i.e. once the clone() call finishes), we don't need the
    // write end of the pipe anymore, so we close it.
    close(pipe_write).context("Failed to close write end of pipe")?;

    // We also create a File wrapping the pipe. This is mostly for convenience, since it lets us
    // use the standard IO traits to talk about doing IO on the pipe.
    //
    // UNSAFE: Ownership of the fd must be transferred to the File. In other words, we can't
    // directly use it (including closing it) later in the program, except through the File.
    // Shadowing it here should help prevent doing that without realizing it.
    let mut pipe_read = unsafe { File::from_raw_fd(pipe_read) };

    // Wait for the child to perform a SIGTRAP; the execve() performed by the child will result in
    // one due to ptrace::traceme().
    wait_for_sigtrap(&child).context("Failed to wait for child to be ready to ptrace")?;

    // Ensure that the child process won't outlive us.
    let child_pid = Pid::from_raw(child.id() as i32);
    ptrace::setoptions(child_pid, ptrace::Options::PTRACE_O_EXITKILL)
        .context("Failed to set child ptrace options")?;

    // Wait for lmcas_instrumentation_setup to be called. This skips past all of libc's setup,
    // which we currently assume we don't need to be worried about.
    let (parent_page_addr, function_pointer_table, noop_sighandler_addr) =
        wait_for_lmcas_instrumentation_setup(&mut child, &mut pipe_read)?;

    // We're in a good state, so go ahead with the main loop!
    let mut tape = Vec::new();
    let result = record_tape(
        &instrumentation_spec.syscall_mocks,
        parent_page_addr,
        function_pointer_table,
        noop_sighandler_addr,
        &mut tape,
        &mut child,
        pipe_read,
    );
    let output_path = args.output_path;
    let write_tape = move || {
        match output_path {
            Some(path) => {
                let mut writer = BufWriter::new(File::create(path)?);
                serde_json::to_writer(&mut writer, &tape)?;
                writer.flush()?;
            }
            None => serde_json::to_writer_pretty(stdout(), &tape)?,
        }
        Ok(())
    };

    match result {
        Ok(()) => write_tape(),

        Err(err)
            if args.no_fail_on_unhandled_syscall
                // TODO: We should refactor to make this work again.
                // && err.root_cause().is::<UnhandledSyscall>()
                    =>
        {
            // We SIGKILL the child, since it's stopped right now at a syscall.
            child.kill()?;

            // We then write the rest of the tape.
            write_tape()
        }

        // If there was an error, we SIGKILL the child before reporting it.
        Err(err) => {
            child.kill()?;
            Err(err)
        }
    }
}

/// Returns the address of the parent page in the child's address space and the table mapping
/// function addresses to annotation IDs.
fn wait_for_lmcas_instrumentation_setup(
    child: &mut Child,
    pipe_read: &mut File,
) -> Result<(u64, HashMap<u64, u64>, u64)> {
    let child_pid = Pid::from_raw(child.id() as i32);

    // In the first part of instrumented execution, we're between the libc's _start and the start
    // of main (where lmcas_instrumentation_setup gets called).
    loop {
        wait_for_next_syscall(child, |_| Ok(()))
            .context("Failed to wait for lmcas_instrumentation_setup to get called")?;
        let regs =
            ptrace::getregs(child_pid).context("Failed to get child registers (before main)")?;

        // Once a write to fd 1023 happens, we know we're in lmcas_instrumentation_setup.
        if regs.orig_rax == (SYS_write as u64) && regs.rdi == 1023 {
            break;
        }
    }

    // At this point, the child should be stopped just after writing the (13-byte) ready message to
    // the IPC pipe. We read it to ensure that the child is behaving as expected.
    read_ready_message(child_pid, pipe_read)
        .context("Failed to read ready message from child process")
}

fn record_tape(
    syscall_mocks: &SyscallMocks,
    parent_page_addr: u64,
    function_pointer_table: HashMap<u64, u64>,
    noop_sighandler_addr: u64,
    tape: &mut Vec<TapeEntry<KnownSyscall>>,
    child: &mut Child,
    mut pipe_read: File,
) -> Result<()> {
    let child_pid = Pid::from_raw(child.id() as i32);

    // For the rest of execution, we run until a syscall is performed. Since we wrap all the
    // syscalls that we support pre-neck, this syscall should always be a write to the pipe.
    loop {
        // Wait for the pipe to be written to.
        wait_for_next_syscall(child, |regs| {
            ensure!(
                regs.orig_rax == (SYS_write as u64),
                concat!(
                    "Unexpected syscall {} (0x{:x}). This is likely a syscall ",
                    "performed in an unsupported way (e.g. inline assembly).\n\n",
                    "rdi = 0x{:x}\n",
                    "rsi = 0x{:x}\n",
                    "rdx = 0x{:x}\n",
                    "r10 = 0x{:x}\n",
                    "r8  = 0x{:x}\n",
                    "r9  = 0x{:x}\n"
                ),
                regs.orig_rax,
                regs.orig_rax,
                regs.rdi,
                regs.rsi,
                regs.rdx,
                regs.r10,
                regs.r8,
                regs.r9,
            );
            ensure!(
                regs.rdi == 1023,
                concat!(
                    "Unexpected write({}, ...) syscall; this is likely a syscall ",
                    "performed in an unsupported way (e.g. inline assembly)"
                ),
                regs.rdi,
            );
            Ok(())
        })?;

        // Parse the message that was written to the pipe and act accordingly.
        let msg_opt = read_tape_entry_message(&mut pipe_read)
            .context("Failed to read message from IPC pipe")?;
        if let Some(msg_without_syscall_result) = msg_opt {
            log::trace!("Got preliminary message: {:?}", msg_without_syscall_result);
            let msg = msg_without_syscall_result.map_syscall_start_result(|()| {
                // If the message is a syscall message, check what the syscall is.
                let (args, mock_regs) = wait_for_next_syscall(child, |regs| {
                    let args = KnownSyscallArgs::from_regs(regs)?;

                    let (mock_regs, new_entry_opt) = syscall_mocks
                        .check_syscall_for_mocking(
                            parent_page_addr,
                            &function_pointer_table,
                            noop_sighandler_addr,
                            child_pid,
                            regs,
                            &args,
                        )
                        .context("Failed to mock syscall")?;
                    match &mock_regs {
                        MockAction::DontMock => {}

                        // Check if the syscall is one that we're mocking by no-opping it. If it
                        // is, replace the syscall with a fake one that will certainly get an
                        // ENOSYS.
                        MockAction::NoOp(_) => {
                            log::debug!("Performing fake syscall to mock");
                            let mut regs = regs;
                            regs.orig_rax = 0x7fff_ffff_ffff_ffff;
                            ptrace::setregs(child_pid, regs)
                                .context("Failed to set up fake syscall while mocking syscall")?;
                        }

                        // If the syscall is one that we replace, we can just set in the new
                        // registers.
                        MockAction::Replace(regs) => {
                            ptrace::setregs(child_pid, *regs)
                                .context("Failed to set up syscall while mocking syscall")?;
                        }
                    }
                    // Add a new tape entry to represent the new syscall we made. This
                    // is done when a mock changes the syscall, and recording
                    // the changed syscall entry requires making an entry to put in.
                    if let Some(new_tape_entry) = new_entry_opt {
                        let new_tape_entry_args = new_tape_entry
                            .map_syscall_start_result(|args| {
                                KnownSyscall::from_regs(
                                    &function_pointer_table,
                                    child_pid,
                                    regs,
                                    args,
                                )
                            })
                            .context("Failed to construct new syscall from syscall args")?;
                        tape.push(new_tape_entry_args)
                    }
                    Ok((args, mock_regs))
                })
                .context("Error when performing syscall notified by syscall wrapper")?;

                // If we're mocking the syscall by no-opping it, set the new registers.
                let regs = match mock_regs {
                    MockAction::DontMock | MockAction::Replace(_) => ptrace::getregs(child_pid)
                        .context("Failed to get child registers after a syscall")?,
                    MockAction::NoOp(mock_regs) => {
                        ptrace::setregs(child_pid, mock_regs)
                            .context("Failed to set child registers to mock a syscall")?;
                        mock_regs
                    }
                };

                // Collect the output information for the syscall.
                KnownSyscall::from_regs(&function_pointer_table, child_pid, regs, args)
                    .context("Failed to decode syscall")
            })?;

            // Append the message to the tape.
            log::debug!("Got message: {:?}", msg);
            tape.push(msg);
        } else {
            // We're done. We can kill the child process and return with success.
            child.kill().context("Failed to kill child process")?;
            return Ok(());
        }
    }
}

/// Performs a single step of running a syscall.
///
/// 1. Allows the child to execute until it tries to perform a syscall, then stops it.
/// 2. Runs the `check_syscall` closure to see if the syscall should be allowed. If not, errors
///    out.
/// 3. Allows the child to perform the syscall, then stops it.
fn wait_for_next_syscall<T>(
    child: &Child,
    mut check_syscall: impl FnMut(user_regs_struct) -> Result<T>,
) -> Result<T> {
    let child_pid = Pid::from_raw(child.id() as i32);

    // Allow the child to run until the next syscall.
    ptrace::syscall(child_pid, None)
        .context("Failed to ask child process to continue until a syscall")?;

    // Wait for it to actually perform one.
    wait_for_sigtrap(child).context("Failed to wait for child to start a syscall")?;

    // Get information about the syscall from the child's registers.
    let regs =
        ptrace::getregs(child_pid).context("Failed to get child registers before a syscall")?;
    log::trace!("Child starting syscall: {:#?}", regs);

    // Check whether the syscall should be allowed.
    let out = check_syscall(regs)?;

    // Allow the child to perform the syscall.
    ptrace::syscall(child_pid, None).context("Failed to ask child process to finish a syscall")?;

    // Wait for it to finish.
    wait_for_sigtrap(child).context("Failed to wait for child to finish a syscall")?;
    log::trace!("Child finished syscall");

    Ok(out)
}

/// Waits until the child process receives a SIGTRAP. If the child process stops for a different
/// reason, errors out.
fn wait_for_sigtrap(child: &Child) -> Result<()> {
    log::trace!("Waiting for child to receive a SIGTRAP...");
    let child_pid = Pid::from_raw(child.id() as i32);
    match waitpid(Some(child_pid), None) {
        Ok(WaitStatus::Stopped(_, Signal::SIGTRAP)) => Ok(()),
        Ok(s) => bail!("Child stopped with status {:?}", s),
        Err(err) => Err(err).context("Failed to wait for child to get a SIGTRAP"),
    }
}
