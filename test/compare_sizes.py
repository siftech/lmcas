#!/usr/bin/env python3

from argparse import ArgumentParser
from pathlib import Path
import struct
from sys import stdout
from typing import Callable, Iterable


def segments(
    contents: bytes,
) -> Iterable[tuple[int, int, int, int, int, int, int, int]]:
    """
    Yields the segments in the program header of an ELF binary, in the order
    they appear in the Elf64_Phdr struct. See `man 5 elf` for details.
    """

    assert len(contents) > 0x40  # Size of ELF header
    assert contents[:6] == b"\x7fELF\x02\x01"  # 64-bit LE ELF

    # Offset of the program header in the file.
    phoff: int = struct.unpack("<Q", contents[0x20:0x28])[0]

    # Number of bytes per entry and number of entries in the program table.
    phentsize: int = struct.unpack("<H", contents[0x36:0x38])[0]
    phnum: int = struct.unpack("<H", contents[0x38:0x3A])[0]

    for i in range(phnum):
        offset = phoff + phentsize * i
        yield struct.unpack("<LLQQQQQQ", contents[offset : offset + 0x38])


def executable_segment_sizes(contents: bytes) -> Iterable[int]:
    """
    Yields the sizes of executable segments in a binary, given its contents.
    """

    for segment in segments(contents):
        phdr_type: int = segment[0]
        flags: int = segment[1]
        filesz: int = segment[5]
        memsz: int = segment[6]

        # Check this this is a loadable segment.
        if phdr_type != 1:
            continue

        # Check that the flags say the segment is executable.
        if (flags & 0b1) == 0:
            continue

        # Check that the filesz and memsz are equal. If they aren't, something
        # really weird's going on; the kernel will zero-pad pages out to the
        # right alignment already, so programs shouldn't need to specify this
        # unless they're doing something really unusual (code in .bss?).
        assert filesz == memsz

        yield filesz


def executable_segment_size(contents: bytes) -> int:
    """
    Returns the sum of sizes of executable segments in a binary, given its
    contents.
    """
    return sum(executable_segment_sizes(contents))


def subdirectories(path: Path) -> Iterable[str]:
    """
    Yields the names of the subdirectories of path.
    """

    return (child.name for child in path.iterdir() if child.is_dir())


def targets(targets_path: Path) -> Iterable[tuple[str, bytes, bytes]]:
    """
    Yields triples of (name, bloated_contents, debloated_contents) for the
    binaries the tests have run on.
    """

    for target in subdirectories(targets_path):
        for binary in subdirectories(targets_path / target):
            binary_dir = targets_path / target / binary
            with (binary_dir / "bloated" / binary).open("rb") as f:
                bloated = f.read()
            with (binary_dir / "debloated" / binary).open("rb") as f:
                debloated = f.read()
            yield (f"{target}/{binary}", bloated, debloated)


def main():
    parser = ArgumentParser(description="Measures the sizes of binaries.")
    parser.add_argument("-C", "--color", action="store_true", default=stdout.isatty())
    parser.add_argument("--metric", help="Only compare a particular metric")
    parser.add_argument(
        "targets_path",
        type=Path,
        help="the path to the debloated-targets directory",
    )
    args = parser.parse_args()

    # The metrics to compare with.
    metrics: list[Callable[[bytes], int]] = [executable_segment_size, len]
    metrics.sort(key=lambda f: f.__name__)

    for metric in metrics:
        if args.metric != None and args.metric != metric.__name__:
            continue

        for name, bloated, debloated in sorted(targets(args.targets_path)):
            before = metric(bloated)
            after = metric(debloated)

            change = (after - before) / before

            # Convert to a +/- percentage.
            sign = "+" if change > 0 else "-"
            change_str = f"{sign}{100 * abs(change):.2f}%"

            # Add color, if requested.
            if args.color:
                color = "32" if change < 0 else "31"
                if abs(change) > 0.1:
                    color += ";1"
                change_str = f"\x1b[{color}m{change_str}\x1b[0m"

            print(f"{metric.__name__}/{name}: {before} -> {after} ({change_str})")


if __name__ == "__main__":
    main()
