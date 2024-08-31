// SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//! See doc/instrumentation-protocol.md for the protocol this implements.

use crate::TapeEntry;
use anyhow::{bail, ensure, Context, Result};
use instrumentation_parent::pod::read_pods;
use nix::unistd::Pid;
use std::{collections::HashMap, io::Read, slice};

/// Reads and parses a ready message, checks that it has the expected PID, and returns the address
/// of the parent page, table mapping function addresses to annotation IDs, and the sighandler_noop function address.
pub fn read_ready_message<R: Read>(
    child_pid: Pid,
    pipe: &mut R,
) -> Result<(u64, HashMap<u64, u64>, u64)> {
    let mut buf = [0; 37];
    pipe.read_exact(&mut buf)
        .context("Failed to read ready message from pipe")?;

    ensure!(buf[0] == b'R', "Got bad tag byte for ready message");
    let msg_pid = (buf[1] as u32)
        | ((buf[2] as u32) << 8)
        | ((buf[3] as u32) << 16)
        | ((buf[4] as u32) << 24);
    ensure!(
        child_pid.as_raw() as u32 == msg_pid,
        "Got unexpected PID in ready message"
    );

    let parent_page_addr = (buf[5] as u64)
        | ((buf[6] as u64) << 8)
        | ((buf[7] as u64) << 16)
        | ((buf[8] as u64) << 24)
        | ((buf[9] as u64) << 32)
        | ((buf[10] as u64) << 40)
        | ((buf[11] as u64) << 48)
        | ((buf[12] as u64) << 56);

    let noop_sighandler_addr = (buf[13] as u64)
        | ((buf[14] as u64) << 8)
        | ((buf[15] as u64) << 16)
        | ((buf[16] as u64) << 24)
        | ((buf[17] as u64) << 32)
        | ((buf[18] as u64) << 40)
        | ((buf[19] as u64) << 48)
        | ((buf[20] as u64) << 56);

    let function_pointer_start = (buf[21] as u64)
        | ((buf[22] as u64) << 8)
        | ((buf[23] as u64) << 16)
        | ((buf[24] as u64) << 24)
        | ((buf[25] as u64) << 32)
        | ((buf[26] as u64) << 40)
        | ((buf[27] as u64) << 48)
        | ((buf[28] as u64) << 56);
    let function_pointer_count = (buf[29] as u64)
        | ((buf[30] as u64) << 8)
        | ((buf[31] as u64) << 16)
        | ((buf[32] as u64) << 24)
        | ((buf[33] as u64) << 32)
        | ((buf[34] as u64) << 40)
        | ((buf[35] as u64) << 48)
        | ((buf[36] as u64) << 56);

    let function_pointer_count = function_pointer_count as usize;
    let mut function_pointer_table = HashMap::<u64, u64>::with_capacity(function_pointer_count);
    read_pods(child_pid, function_pointer_start)
        .take(function_pointer_count)
        .map(|result| {
            let (addr, annot) = result?;
            assert!(!function_pointer_table.contains_key(&addr));
            function_pointer_table.insert(addr, annot);
            Ok(())
        })
        .collect::<Result<()>>()?;

    Ok((
        parent_page_addr,
        function_pointer_table,
        noop_sighandler_addr,
    ))
}

/// Reads a message other than a ready message from the given pipe. Returns `None` on a done
/// message.
pub fn read_tape_entry_message<R: Read>(pipe: &mut R) -> Result<Option<TapeEntry<()>>> {
    let mut tag_byte = 0;
    pipe.read_exact(slice::from_mut(&mut tag_byte))
        .context("Failed to read tag byte from pipe")?;
    match tag_byte {
        b'D' => Ok(None),
        b'B' => {
            let mut body = [0; 8];
            pipe.read_exact(&mut body)
                .context("Failed to read body of basic block start message from pipe")?;
            let basic_block_id = (body[0] as u64)
                | ((body[1] as u64) << 8)
                | ((body[2] as u64) << 16)
                | ((body[3] as u64) << 24)
                | ((body[4] as u64) << 32)
                | ((body[5] as u64) << 40)
                | ((body[6] as u64) << 48)
                | ((body[7] as u64) << 56);
            let basic_block_id = basic_block_id.into();
            Ok(Some(TapeEntry::BasicBlockStart { basic_block_id }))
        }
        b'C' => {
            let mut body = 0;
            pipe.read_exact(slice::from_mut(&mut body))
                .context("Failed to read body of call info message from pipe")?;
            let start = match body {
                b's' => true,
                b'e' => false,
                _ => bail!("Invalid body of call info message: 0x{:02x}", body),
            };
            Ok(Some(TapeEntry::CallInfo { start }))
        }
        b'S' => Ok(Some(TapeEntry::SyscallStart(()))),
        b'r' => Ok(Some(TapeEntry::Ret)),
        b'c' => {
            let mut body = 0;
            pipe.read_exact(slice::from_mut(&mut body))
                .context("Failed to read body of condbr message from pipe")?;
            let taken = match body {
                0 => false,
                1 => true,
                _ => bail!("Invalid body of condbr message: 0x{:02x}", body),
            };
            Ok(Some(TapeEntry::CondBr { taken }))
        }
        b's' => {
            let mut body = [0; 8];
            pipe.read_exact(&mut body)
                .context("Failed to read body of switch message from pipe")?;
            let value = (body[0] as u64)
                | ((body[1] as u64) << 8)
                | ((body[2] as u64) << 16)
                | ((body[3] as u64) << 24)
                | ((body[4] as u64) << 32)
                | ((body[5] as u64) << 40)
                | ((body[6] as u64) << 48)
                | ((body[7] as u64) << 56);
            let value = value.into();
            Ok(Some(TapeEntry::Switch { value }))
        }
        b'i' => todo!(),
        b'u' => todo!(),
        _ => bail!("Unknown tag byte {:02x}", tag_byte),
    }
}
