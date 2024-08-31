#!/usr/bin/env python3

from .ar import Archive
from .elf import ELF, Symbol
from itertools import chain
import sys
from typing import Iterable


def symbols(path: str) -> Iterable[tuple[ELF, Symbol]]:
    def elf_symbols(elf: ELF) -> Iterable[tuple[ELF, Symbol]]:
        for symbol in elf.symbols:
            yield (elf, symbol)

    if Archive.is_from_file(path):
        return chain(
            *(
                elf_symbols(ELF.from_bytes(header.ident, data))
                for (header, data) in Archive.from_file(path).files
            )
        )
    elif ELF.is_from_file(path):
        return elf_symbols(ELF.from_file(path))
    else:
        raise Exception(f"Could not recognize {path}")


def find_unresolved(symbols: Iterable[tuple[ELF, Symbol]]) -> dict[str, list[str]]:
    defined = set()
    wanted: dict[bytes, list[ELF]] = {}

    for elf, symbol in symbols:
        name = elf.string_from_section(symbol.name, symbol.strtab_index)
        if symbol.section_index == 0:
            if name not in wanted:
                wanted[name] = []
            wanted[name].append(elf)
        else:
            defined.add(name)

    return {
        name.decode("ASCII"): [elf.name for elf in elves]
        for (name, elves) in wanted.items()
        if name not in defined
    }


def main() -> None:
    print(
        ",".join(
            find_unresolved(
                (elf, symbol)
                for path in sys.argv[1:]
                for (elf, symbol) in symbols(path)
                if symbol.binding == 1  # STB_GLOBAL
            )
        )
    )


if __name__ == "__main__":
    main()
