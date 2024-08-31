#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import click
from functools import partial
from hashlib import sha256
from lmcas_sh import sh
import logging
from os import walk
from pathlib import Path
from shutil import copyfile, copymode, move
from tempfile import NamedTemporaryFile, TemporaryDirectory
from typing import Callable, Literal, Iterator, Optional

logger = logging.getLogger("fix_bitcode_paths")


@click.command()
@click.argument(
    "out_dir", type=click.Path(dir_okay=True, file_okay=False, path_type=str)
)
def main(out_dir: str) -> None:
    """
    Finds all ELF files (including shared objects and object files) in OUT_DIR
    and in ar archives (i.e. static libraries) in OUT_DIR and updates paths
    left by GLLVM to reference paths in OUT_DIR, copying bitcode files into it
    if necessary.

    This should be idempotent.
    """

    logging.basicConfig(level=logging.DEBUG)  # TODO: Make this configurable
    out_dir_path = Path(out_dir)
    fixup_files(out_dir_path, partial(do_fixup_path, out_dir_path))

    # TODO: Should this return non-zero on error? Probably, but that
    # potentially breaks builds that do "weird things" that would otherwise
    # work for LMCAS' purposes.


def find_files(search_dir: Path) -> Iterator[tuple[Path, Literal["ar", "elf"]]]:
    """
    Finds ELF and ar files under search_dir, yielding the path to the file and
    its filetype.
    """

    # This intentionally doesn't follow symbolic links -- the only cases where
    # it's valid to have a symbolic link in a Nix output are when the link
    # points to another derivation (which should already have been fixed up by
    # this tool, and if it wasn't, we can't do much about it), and when it
    # points to another output of this derivation, which will get processed by
    # this tool later.
    for dirpath, _dirnames, filenames in walk(search_dir):
        for filename in filenames:
            path = Path(dirpath) / filename
            filetype: Optional[Literal["ar", "elf"]] = None

            # All the signatures we currently care about occur within the first
            # 8 bytes of the file, so we can just read that in for now.
            try:
                with path.open("rb") as f:
                    start = f.read(8)
            except Exception as exc:
                logger.error(f"Failed to open {path}: {exc}")
                continue

            if start.startswith(b"!<arch>\n"):
                filetype = "ar"
            elif start.startswith(b"\x7fELF"):
                filetype = "elf"

            if filetype is not None:
                yield (path, filetype)


def fixup_ar(archive_path: Path, do_fixup_path: Callable[[str], str]) -> None:
    """
    Fixes up paths of ELF files (and nested ar archives, but that's terrible)
    in the ar archive at archive_path.
    """

    # TODO: This could work entirely in-place, which would save a bit of
    # performance and make the code a bit cleaner to read.
    with TemporaryDirectory() as srcdir_str, TemporaryDirectory() as destdir_str:
        srcdir = Path(srcdir_str)
        destdir = Path(destdir_str)

        print(f"Extracting archive {archive_path} to {srcdir_str}")
        sh.ar("x", archive_path, cwd=srcdir)

        dot_o_files = []
        for root, _, files in walk(srcdir):
            for file in files:
                path = Path(root) / file
                dot_o_files.append(path)

        names = []

        def transform_elf(elf_path: Path) -> str:
            "Fixes the paths in a .o file, then moves it to the destdir."

            print(f"Transforming ELF: {elf_path}")
            fixup_elf(elf_path, do_fixup_path)
            move(elf_path, destdir / elf_path.name)
            return elf_path.name

        for path_str in dot_o_files:
            path = Path(path_str)
            name = transform_elf(path)
            names.append(name)

        with TemporaryDirectory() as final_ar_dir:
            final_ar = Path(final_ar_dir) / "archive.a"

            print("Archiving again")
            sh.ar("r", final_ar, *names, cwd=destdir)

            copymode(archive_path, final_ar)
            move(final_ar, archive_path)


def fixup_elf(elf_path: Path, do_fixup_path: Callable[[str], str]) -> None:
    """
    Fixes up paths in the .llvm_bc section of an ELF file at path with the
    do_fixup_path function.
    """

    # First, extract the .llvm_bc section to a file. If it does not exist, this
    # will succeed, but the file will be of zero length.
    with NamedTemporaryFile(mode="w+") as section_file:
        sh.objcopy(
            elf_path,
            section_file.name,
            # We only want this section, since it's the one GLLVM has put paths
            # into.
            "--only-section=.llvm_bc",
            # The binary format is essentially "no container," i.e. "just put
            # the raw bytes in the file."
            "--output-target=binary",
            # This is necessary to get any output -- the bitcode paths don't
            # end up in the memory image of the program when run (that's the
            # alloc flag), and objcopy only copies bytes to the binary format
            # when they're present in the memory image.
            "--set-section-flags",
            ".llvm_bc=alloc",
        )

        # Next, we read in the paths. They're newline-*terminated*, not
        # *separated*.
        #
        # TODO: Shouldn't these be null-terminated instead? What happens when a
        # filename contains a newline?
        paths = section_file.read().splitlines(keepends=True)
        assert all(path.endswith("\n") for path in paths)
        paths = [path[:-1] for path in paths]

        # Next, we can actually fix up each path, then write the paths back to
        # the section file.
        fixed_paths = [do_fixup_path(path) for path in paths]
        if paths == fixed_paths:
            # We can exit early here, saving a bit of time modifying the binary.
            return
        section_file.seek(0)
        section_file.write("".join(path + "\n" for path in fixed_paths))
        section_file.truncate()

        # Finally, we insert the section back into the binary.
        sh.objcopy(
            elf_path, elf_path, "--update-section", f".llvm_bc={section_file.name}"
        )


def fixup_files(search_dir: Path, do_fixup_path: Callable[[str], str]) -> None:
    """
    Fixes up ELF files and ar files under search_dir according to do_fixup_path.
    """

    for path, filetype in find_files(search_dir):
        try:
            if filetype == "ar":
                fixup_ar(path, do_fixup_path)
            elif filetype == "elf":
                fixup_elf(path, do_fixup_path)
        except Exception as exc:
            logger.error(f"Failed to fixup {filetype} file at {path}: {exc}")
            raise


def do_fixup_path(out_dir: Path, path_str: str) -> str:
    """
    If path_str is a path under out_dir, returns it. Otherwise, copies it into
    out_dir and returns the new path.
    """

    path = Path(path_str)
    if path.is_relative_to(out_dir):
        return path_str

    # TODO: We should check if the directory exists and create a new one if so.
    # On the other hand, hopefully nobody is referencing LMCAS in their project
    # accidentally?
    bitcode_dir = out_dir / ".lmcas-bitcode"
    bitcode_dir.mkdir(exist_ok=True, parents=True)

    # We hash the file to ensure that we generate non-conflicting filenames.
    hasher = sha256()
    with path.open("rb") as f:
        # TODO: Maybe chunk this if peak memory usage is a problem?
        hasher.update(f.read())
    bitcode_hash = hasher.hexdigest()

    # Then, we copy the file to the hash-based filename and return that filename.
    out_path = bitcode_dir / f"{bitcode_hash}.bc"
    copyfile(path, out_path)
    return str(out_path)
