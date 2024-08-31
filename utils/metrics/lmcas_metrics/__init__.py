# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from pathlib import Path
import re
from lmcas_sh import sh


def text_section_size(binary: Path) -> int:
    # Extract the .text section of the binary.
    text_bytes = sh.with_stdout_bytes.objcopy(
        binary, "/dev/null", "--dump-section", ".text=/dev/stdout"
    )
    return len(text_bytes)


def instruction_count(binary: Path) -> int:
    return sum(instruction_count_of_functions(binary).values())


def function_count(binary: Path) -> int:
    return len(instruction_count_of_functions(binary))


def instruction_count_of_functions(binary: Path) -> dict[str, int]:
    """
    Scans the .text section of a binary to report statistics about the code
    with it.

    Returns a dictionary mapping function names to instruction counts.
    """

    output = sh.with_stdout.objdump("-dj", ".text", binary)
    functions_to_instruction_counts = {}
    current_function = None

    # The objdump output looks something like:

    # /some/path: file format ...
    # ...
    #
    # 0000000000418c20 <execute_command_internal.cold>:
    #   418c20:       48 89 84 24 e0 00 00    mov    %rax,0xe0(%rsp)
    #   418c27:       00
    #   418c28:       48 8b 04 25 08 00 00    mov    0x8,%rax
    #   418c2f:       00
    #   418c30:       0f 0b                   ud2
    #
    # ...

    # Note that some of the above is tabs; the address each instruction is at
    # has a tab after the colon, and the bytes of each instruction end with a
    # space and a tab.

    # We'll precompile some regexes for matching each of the kinds of lines we
    # care about.
    function_header_regex = re.compile("[0-9a-f]{16} <([^>]+)>:")
    instruction_regex = re.compile("  [0-9a-f]+:\t[0-9a-f ]+\t.+")
    continuation_regex = re.compile("  [0-9a-f]+:\t[0-9a-f ]+")

    for line in output.splitlines():
        function_header_matches = function_header_regex.fullmatch(line)
        instruction_regex_matches = instruction_regex.fullmatch(line)
        continuation_regex_matches = continuation_regex.fullmatch(line)

        # We're always on the lookout for new header lines. If we find one,
        # switch the current function to it.
        if function_header_matches is not None:
            current_function = function_header_matches.group(1)
            functions_to_instruction_counts[current_function] = 0

        # We skip the block at the top of the output by keeping track of the
        # function header lines. Until we've reached the first one, we just
        # ignore the line.
        elif current_function is None:
            pass

        # There are blank lines between headers. We want to just skip these.
        elif line == "":
            pass

        # If it's an instruction, we increment the relevant instruction counts.
        elif instruction_regex_matches is not None:
            functions_to_instruction_counts[current_function] += 1

        # If it's a continuation line for an instruction that's longer than 7
        # bytes, we ignore it, since we don't want to double-count long
        # instructions.
        elif continuation_regex_matches is not None:
            pass

        # Otherwise, bail out!
        else:
            raise Exception(f"Unrecognized line from objdump: {line!r}")

    return functions_to_instruction_counts


ROP_GADGET_PATTERN = re.compile(
    r".*[^\d](?P<num>\d+) gadgets found.*", re.MULTILINE | re.DOTALL
)


def rop_gadget_count(binary: Path) -> int:
    output = sh.with_stdout.ropper(
        # Only concerned with ROP gadgets (not JOP or SYS)
        "--type",
        "rop",
        # Binary path
        "--file",
        binary,
        # Architecture
        "--arch",
        "x86_64",
    )

    matches = ROP_GADGET_PATTERN.match(output)
    if matches is None:
        raise Exception(f"Could not find any matches in text:\n{output}")

    num_gadgets = int(matches.group("num"))
    return num_gadgets
