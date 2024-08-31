from dataclasses import dataclass
from pathlib import Path


@dataclass
class FileHeader:
    ident: str
    mtime: int
    uid: int
    gid: int
    mode: int
    size: int

    @staticmethod
    def from_bytes(contents: bytes, offset: int) -> "FileHeader":
        assert offset + 60 < len(contents)
        ident = contents[offset : offset + 16].rstrip(b" ").decode("ASCII")

        if ident == "//":
            assert contents[offset + 16 : offset + 48] == b" " * 32
            mtime = uid = gid = mode = 0
        else:
            mtime = int(contents[offset + 16 : offset + 28].rstrip(b" "), 10)
            uid = int(contents[offset + 28 : offset + 34].rstrip(b" "), 10)
            gid = int(contents[offset + 34 : offset + 40].rstrip(b" "), 10)
            mode = int(contents[offset + 40 : offset + 48].rstrip(b" "), 8)

        size = int(contents[offset + 48 : offset + 58].rstrip(b" "), 10)
        assert contents[offset + 58 : offset + 60] == b"\x60\x0a"
        return FileHeader(ident, mtime, uid, gid, mode, size)


@dataclass(frozen=True)
class Archive:
    ident: bytes
    files: list[tuple[FileHeader, bytes]]

    @staticmethod
    def is_from_file(path: Path | str) -> bool:
        with Path(path).open("rb") as f:
            contents = f.read(8)
        return Archive.is_from_bytes(contents)

    @staticmethod
    def from_file(path: Path | str) -> "Archive":
        with Path(path).open("rb") as f:
            contents = f.read()
        return Archive.from_bytes(contents)

    @staticmethod
    def is_from_bytes(contents: bytes) -> bool:
        return len(contents) >= 8 and contents[:8] == b"!<arch>\n"

    @staticmethod
    def from_bytes(contents: bytes) -> "Archive":
        ident = contents[:8]
        files = []

        string_section = None

        i = 8
        while i < len(contents):
            header = FileHeader.from_bytes(contents, i)
            i += 60
            data = contents[i : i + header.size]
            i += header.size

            # Check that the section is appropriately padded.
            if i % 2 != 0:
                assert contents[i] == 10
                i += 1

            if header.ident == "/":
                pass  # quietly ignore it...
            elif header.ident == "//":
                assert string_section is None
                string_section = data
            else:
                files.append((header, data))

        if string_section is not None:
            for header, _ in files:
                if header.ident.startswith("/"):
                    start = end = int(header.ident[1:])
                    while string_section[end] != 10:
                        end += 1
                    header.ident = string_section[start:end].decode("ASCII")

                assert header.ident.endswith("/")
                header.ident = header.ident[: len(header.ident) - 1]

        return Archive(ident, files)
