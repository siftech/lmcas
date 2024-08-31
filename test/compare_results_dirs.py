#!/usr/bin/env python3

from argparse import ArgumentParser
from dataclasses import dataclass
from pathlib import Path
import struct
from sys import stdout
from typing import Callable, Iterable, TextIO


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


def targets(targets_path: Path) -> Iterable[tuple[str, int, int]]:
    """
    Yields triples of (name, bloated_size, debloated_size) for the binaries the
    tests have run on.
    """

    for target in subdirectories(targets_path):
        for binary in subdirectories(targets_path / target):
            binary_dir = targets_path / target / binary
            with (binary_dir / "bloated" / binary).open("rb") as f:
                bloated = f.read()
            with (binary_dir / "debloated" / binary).open("rb") as f:
                debloated = f.read()
            yield (
                f"{target}/{binary}",
                executable_segment_size(bloated),
                executable_segment_size(debloated),
            )


def targets_between(
    before_path: Path, after_path: Path
) -> Iterable[tuple[str, int, int, int]]:
    """
    Yields 4-tuples of (name, bloated_size, before_debloated_size,
    after_debloated_size) for the binaries the tests have run on.
    """

    for (
        (before_name, before_bloated, before_debloated),
        (after_name, after_bloated, after_debloated),
    ) in zip(sorted(targets(before_path)), sorted(targets(after_path))):
        assert before_name == after_name
        assert before_bloated == after_bloated
        yield (before_name, before_bloated, before_debloated, after_debloated)


@dataclass
class Cell:
    """
    A cell in a table.
    """

    value: str
    color: str | None = None

    def __len__(self) -> int:
        return len(self.value)

    def __str__(self) -> str:
        if self.color == None:
            return self.value
        return f"\x1b[{self.color}m{self.value}\x1b[0m"

    @staticmethod
    def change(before: int, after: int) -> "Cell":
        """
        Returns a cell representing a percentage change.
        """

        change = (after - before) / before
        color = "32" if change < 0 else "31"
        if abs(change) > 0.1:
            color += ";1"
        sign = "+" if change > 0 else "-"
        return Cell(f"{sign}{100 * abs(change):.2f}%", color)


def print_row(row: list[Cell], column_widths: list[int], file: TextIO) -> None:
    """
    Prints a single row of a table.
    """

    assert len(row) == len(column_widths)
    pad = column_widths[0] - len(row[0])
    print(row[0], end=" " * pad, file=file)

    for i, cell in enumerate(row):
        if i == 0:
            continue

        pad = column_widths[i] - len(cell)
        print(" | " + (" " * pad) + str(cell), end="", file=file)

    print(file=file)


def print_table(table: list[list[Cell]], file: TextIO = stdout) -> None:
    """
    Formats and prints a table. The first column is left-aligned; other columns
    are right-aligned.
    """

    # Check that the table is rectangular.
    assert len(table) != 0
    column_count = len(table[0])
    assert column_count != 0
    for row in table:
        assert len(row) == column_count

    # Compute the widths of each column.
    column_widths = [max(len(row[i]) for row in table) for i in range(column_count)]

    # Print the header.
    print_row(table[0], column_widths, file)

    # Print a line between the header and the rest of the table.
    print("-" * column_widths[0], end="", file=file)
    for width in column_widths[1:]:
        print("-+-" + ("-" * width), end="", file=file)
    print(file=file)

    # Print each row.
    for row in table[1:]:
        print_row(row, column_widths, file)


def make_table(before_path: Path, after_path: Path) -> Iterable[list[Cell]]:
    """
    Makes a table of results.
    """

    yield [
        Cell(s, "1")
        for s in ["Name", "Bloated", "Before", "After", "Bl->Be", "Bl->Af", "Be->Af"]
    ]

    for (name, bloated, before, after) in sorted(
        targets_between(before_path, after_path), key=lambda row: row[1], reverse=True
    ):
        yield [
            Cell(name),
            Cell(str(bloated)),
            Cell(str(before)),
            Cell(str(after)),
            Cell.change(bloated, before),
            Cell.change(bloated, after),
            Cell.change(before, after),
        ]


def main():
    parser = ArgumentParser(
        description="Measures the improvement in executable segment size made between commits"
    )
    parser.add_argument(
        "before_path",
        type=Path,
        help="the path to the debloated-targets directory before the change being measured",
    )
    parser.add_argument(
        "after_path",
        type=Path,
        help="the path to the debloated-targets directory after the change being measured",
    )
    args = parser.parse_args()

    print_table(list(make_table(args.before_path, args.after_path)))


if __name__ == "__main__":
    main()
