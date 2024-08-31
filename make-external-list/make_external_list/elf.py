from dataclasses import dataclass, field
from pathlib import Path
import struct
from typing import Optional


@dataclass(frozen=True)
class File:
    ident: bytes
    file_type: int
    machine: int
    version: int
    entry: int
    program_header_offset: int
    section_header_offset: int
    flags: int
    elf_header_entry_size: int
    program_header_entry_size: int
    program_header_count: int
    section_header_entry_size: int
    section_header_count: int
    section_header_strings_index: int

    @staticmethod
    def from_bytes(contents: bytes) -> "File":
        assert len(contents) > 64  # Size of ELF header
        assert contents[:6] == b"\x7fELF\x02\x01"  # 64-bit LE ELF
        return File(contents[:16], *struct.unpack("<HHLQQQLHHHHHH", contents[16:64]))


@dataclass(frozen=True)
class Section:
    name: int
    section_type: int
    flags: int
    addr: int
    offset: int
    size: int
    link: int
    info: int
    addr_align: int
    entry_size: int

    @staticmethod
    def from_bytes(contents: bytes, offset: int) -> "Section":
        return Section(*struct.unpack("<LLQQQQLLQQ", contents[offset : offset + 64]))


@dataclass(frozen=True)
class Symbol:
    name: int
    symbol_type: int
    binding: int
    visibility: int
    section_index: int
    value: int
    size: int
    strtab_index: int

    @staticmethod
    def from_bytes(contents: bytes, offset: int, strtab_index: int) -> "Symbol":
        name, info, visibility, section_index, value, size = struct.unpack(
            "<LBBHQQ", contents[offset : offset + 24]
        )
        symbol_type = info & 0x0F
        binding = (info & 0xF0) >> 4

        return Symbol(
            name,
            symbol_type,
            binding,
            visibility,
            section_index,
            value,
            size,
            strtab_index,
        )


@dataclass(frozen=True)
class ELF:
    name: str
    contents: bytes = field(repr=False)
    file: File
    sections: list[Section]
    symbols: list[Symbol]

    @staticmethod
    def is_from_file(path: Path | str) -> bool:
        with Path(path).open("rb") as f:
            contents = f.read(4)
        return ELF.is_from_bytes(contents)

    @staticmethod
    def from_file(path: Path | str) -> "ELF":
        with Path(path).open("rb") as f:
            contents = f.read()
        return ELF.from_bytes(str(path), contents)

    @staticmethod
    def is_from_bytes(contents: bytes) -> bool:
        return len(contents) >= 4 and contents[:4] == b"\x7fELF"

    @staticmethod
    def from_bytes(name: str, contents: bytes) -> "ELF":
        file = File.from_bytes(contents)

        sections: list[Section] = []
        for i in range(file.section_header_count):
            offset = file.section_header_offset + file.section_header_entry_size * i
            sections.append(Section.from_bytes(contents, offset))

        symbols: list[Symbol] = []
        for section in sections:
            if section.section_type != 2:  # SHT_SYMTAB
                continue
            assert section.entry_size != 0
            assert section.size % section.entry_size == 0

            for offset in range(0, section.size, section.entry_size):
                offset += section.offset
                symbols.append(Symbol.from_bytes(contents, offset, section.link))

        return ELF(name, contents, file, sections, symbols)

    def string_from_section(self, n: int, section_index: int) -> bytes:
        assert section_index < len(self.sections)
        section = self.sections[section_index]
        assert section.section_type == 3  # STRTAB
        assert n < section.size
        start = end = section.offset + n
        while True:
            assert end < section.offset + section.size
            if self.contents[end] == 0:
                return self.contents[start:end]
            end += 1

    def section_string(self, n: int) -> Optional[bytes]:
        shstrndx = self.file.section_header_strings_index

        if shstrndx == 0:  # SHN_UNDEF
            return None
        assert len(self.sections) != 0

        if shstrndx == 0xFFFF:  # SHN_XINDEX
            shstrndx = self.sections[0].link
        else:
            assert self.sections[0].link == 0

        return self.string_from_section(n, shstrndx)
