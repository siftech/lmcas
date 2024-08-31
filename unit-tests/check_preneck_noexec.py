# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import gdb
from pathlib import Path


class LmcasTestCommand(gdb.Command):
    def __init__(self):
        super(LmcasTestCommand, self).__init__("lmcas_test", gdb.COMMAND_DATA)

    def invoke(self, arg: str, from_tty: bool):
        # Run until we reach _tabacco_at_neck.
        gdb.Breakpoint("_tabacco_at_neck")
        gdb.execute("run -h")
        assert gdb.selected_frame().name() == "_tabacco_at_neck"

        # Find all the symbols on the stack.
        stack_frames = set()
        frame = gdb.selected_frame()
        while frame is not None:
            stack_frames.add(frame.name())
            frame = frame.older()

        # Finish the _tabacco_at_neck function.
        gdb.execute("finish")

        # Find all the symbols whose names start with "_tabacco", _other than_
        # those on the stack.
        info_functions = gdb.execute("info functions", to_string=True)
        assert info_functions is not None
        functions = [
            (chunk[1], int(chunk[0], 16))
            for chunk in (line.split() for line in info_functions.splitlines())
            if len(chunk) > 0
            if chunk[1].startswith("_tabacco")
            if chunk[1] not in stack_frames
        ]

        # Read the program's memory map.
        inferior = gdb.selected_inferior()
        with (Path("/proc") / str(inferior.pid) / "maps").open() as f:
            ranges = [
                range(*[int(chunk, 16) for chunk in line.split()[0].split("-")])
                for line in f.read().splitlines()
            ]

        def is_mapped(addr: int) -> bool:
            "Checks whether the address is mapped."

            for addr_range in ranges:
                if addr in addr_range:
                    return True
            return False

        def is_int3(addr: int) -> bool:
            "Returns whether an address contains an INT3 instruction."

            return inferior.read_memory(addr, 1).tobytes() == b"\xcc"

        # Check that the first byte of every function is either not mapped or
        # is an INT3 instruction. Ideally we'd check every byte, but the gdb
        # API doesn't seem to expose the sizes of functions...
        for (name, addr) in functions:
            print(f"Checking {name!r}...")
            assert not is_mapped(addr) or is_int3(addr)

        # Mark the test as having succeeded.
        Path(arg).mkdir()
        gdb.execute("quit")


LmcasTestCommand()
