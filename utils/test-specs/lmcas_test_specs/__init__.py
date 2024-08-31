# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from dataclasses import dataclass
from lmcas_utils import unwrap_or, fallback_list, option_map
from lmcas_dejson import from_json
from pathlib import Path
import toml
import re
from typing import Literal, Union, Optional, Pattern


@dataclass
class TestSpec:
    """
    A specification for a single test of the binary. The behavior is determined
    by the kind field.

    When the kind is "simple-run", the following fields are also applicable:

    argv0: The value to pass as argv[0]. Defaults to the actual path to the
           binary being run.
    args: The arguments to pass after argv[0]. Defaults to no arguments.
    env: Environment variables to be set. Defaults to the empty set.
    stdin: Input to send on stdin. Defaults to the empty string.
    expected_status: The status code the process should exit with, or False to
                     not check the status code. Defaults to 0.
    expected_stdout: The data expected on stdout, or False to not check stdout.
                     Defaults to the empty string.
    expected_stderr: The data expected on stderr, or False to not check stderr.
                     Defaults to the empty string.

    When the kind is "contains-symbol", the following fields are also applicable:

    symbol_name: The symbol to find. Required.

    When the kind is "links-to-symbol", the following fields are also applicable:

    symbol_name: The symbol to find. Required.

    When the kind is "text-section-size", the following fields are also applicable:

    expected_size_bloated: The expected size of the bloated binary's text
                           section, in bytes.
    expected_size_debloated: The expected size of the debloated binary's text
                             section, in bytes.

    When the kind "instruction-count", the following fields are also applicable:

    expected_size_bloated: The expected number of instructions in the bloated
                           binary.
    expected_size_debloated: The expected number of instructions in the
                             debloated binary.

    When the kind is "instruction-count-of-function", the following fields are
    also applicable:

    symbol_name: The name of the function whose instructions are being counted.
    expected_size_bloated: The expected number of instructions in the function
                           in the bloated binary.
    expected_size_debloated: The expected number of instructions in the
                             function in the debloated binary.

    When the kind is "function-count", the following fields are also applicable:

    expected_size_bloated: The expected number of functions in the bloated
                           binary.
    expected_size_debloated: The expected number of functions in the debloated
                             binary.

    When the kind is "rop-gadget-count", the following fields are also applicable:

    expected_size_bloated: The expected number of ROP gadgets in the bloated
                           binary.
    expected_size_debloated: The expected number of ROP gadgets in the
                             debloated binary.

    When the kind is any of "text-section-size", "instruction-count",
    "instruction-count-of-function", "function-count", "rop-gadget-count", the
    following fields are also applicable:

    size_leeway: The fraction of the value that the value can be wrong by. For
                 example, if the expected value is 1000 and the actual value is
                 900, size_leeway would have to be at least 0.1 for the test to
                 pass. Defaults to 0.
    pos_size_leeway: Overrides size_leeway in the positive direction. Defaults
                     to size_leeway.
    neg_size_leeway: Overrides size_leeway in the negative direction. Defaults
                     to size_leeway.
    abs_size_leeway: The amount by which the value can be wrong by. For example,
                     if the expected value is 1000 and the actual value is 900,
                     abs_size_leeway would have to be at least 100 for the test
                     to pass. Defaults to 0.
    pos_abs_size_leeway: Overrides abs_size_leeway in the positive direction.
                         Defaults to abs_size_leeway.
    neg_abs_size_leeway: Overrides abs_size_leeway in the negative direction.
                         Defaults to abs_size_leeway.

    However, only one set of {size_leeway, pos_size_leeway, neg_size_leeway}
    and {abs_size_leeway, pos_abs_size_leeway, neg_abs_size_leeway} may be
    included.
    """

    kind: Literal[
        "simple-run",
        "contains-symbol",
        "links-to-symbol",
        "text-section-size",
        "instruction-count",
        "instruction-count-of-function",
        "function-count",
        "rop-gadget-count",
    ]
    """ The kind of test this is. The interpretation of the fields not described
    in this block depends on the value of this field. """

    should_pass_bloated: bool = True
    """ Whether the test should pass when run on the original, bloated
    binary. """

    should_pass_debloated: bool = True
    """ Whether the test should pass when run on the debloated binary."""

    fixtures: Optional[list[str]] = None
    """ A list of fixtures that should be required, for example for tests
              that need specific filesystem state."""

    keywords: Optional[list[str]] = None
    # TODO: Is this correct? Pretty sure pytest's keyword functionality is like
    # a full blown pattern language...
    """ A list of marks this test should have. (Marks are the closest pytest
    equivalent to PRT's keywords.)"""

    broken: bool = None
    """A string with a link to the Trac issue for tracking why a test is
    broken. """

    argv0: Optional[str] = None
    args: Optional[list[str]] = None
    env: Optional[dict[str, str]] = None
    stdin: str = ""
    expected_status: Union[int, Literal[False]] = 0
    is_stdout_regex: bool = False
    expected_stdout: Union[str, Literal[False]] = ""
    is_stderr_regex: bool = False
    expected_stderr: Union[str, Literal[False]] = ""
    symbol_name: Optional[str] = None
    expected_size_bloated: Optional[int] = None
    expected_size_debloated: Optional[int] = None
    size_leeway: Optional[float] = None
    pos_size_leeway: Optional[float] = None
    neg_size_leeway: Optional[float] = None
    abs_size_leeway: Optional[float] = None
    pos_abs_size_leeway: Optional[float] = None
    neg_abs_size_leeway: Optional[float] = None


@dataclass
class TargetSpec:
    """
    A specification of a series of tests to run against a binary, before and
    after debloating.
    """

    tests: Optional[dict[str, TestSpec]] = None
    """ The tests to perform on the bloated and debloated binaries. """

    bin_name: Optional[str] = None
    """ The name of the binary to test. This is the name the test binaries will
    be created as. Defaults to the target name. """

    input_binary: Optional[Path] = None
    """ The path to the input binary, relative to the test's input_binary_dir.
    Defaults to bin_name."""

    spec: Optional[Path] = None
    """ The path to the debloating specification file, relative to the test's
          spec_dir. Defaults to the name of the target with ".json"
          appended."""


def parse_targets_file(toml_spec_file: Path) -> dict[str, TargetSpec]:
    """
    Reads a TOML file, and creates a TargetSpec from it.

    :param toml_spec_file: Path to the toml file that contains target
    specifications
    """

    with toml_spec_file.open() as f:
        return parse_targets_file_from_value(toml.load(f))


def parse_targets_file_from_value(toml_spec: object) -> dict[str, TargetSpec]:
    """
    Creates a TargetSpec from a value (typically read from a TOML file).
    """

    # Load the target specifications.
    target_specs: dict[str, TargetSpec] = from_json(dict[str, TargetSpec], toml_spec)

    for target_spec_name, target_spec in target_specs.items():
        target_tests: dict[str, TestSpec] = unwrap_or(target_spec.tests, {})
        target_spec.bin_name = unwrap_or(target_spec.bin_name, target_spec_name)
        target_spec.input_binary = unwrap_or(
            target_spec.input_binary, Path(target_spec.bin_name)
        )
        target_spec.spec = unwrap_or(target_spec.spec, Path(f"{target_spec_name}.json"))

        for test_spec in target_tests.values():
            test_spec.env = unwrap_or(test_spec.env, dict[str, str]())
            if test_spec.kind not in [
                "simple-run",
                "contains-symbol",
                "links-to-symbol",
                "text-section-size",
                "instruction-count",
                "instruction-count-of-function",
                "function-count",
                "rop-gadget-count",
            ]:
                raise TypeError(f"Invalid test kind {test_spec.kind!r}")

            keywords: list[str] = unwrap_or(test_spec.keywords, [])
            if "broken" in keywords:
                raise Exception(
                    "Use the broken field to mark brokenness, not the keywords field"
                )
            test_spec.keywords = keywords

            if test_spec.kind in [
                "contains-symbol",
                "links-to-symbol",
                "instruction-count-of-function",
            ]:
                if test_spec.symbol_name is None:
                    raise TypeError(
                        f"symbol_name is required for a {test_spec.kind} test"
                    )
            if test_spec.kind in [
                "text-section-size",
                "instruction-count",
                "instruction-count-of-function",
                "function-count",
                "rop-gadget-count",
            ]:
                if None in [
                    test_spec.expected_size_bloated,
                    test_spec.expected_size_debloated,
                ]:
                    raise TypeError(
                        "expected_size_bloated and expected_size_debloated are"
                        f" required for a {test_spec.kind} test"
                    )
                _valid_leeways(test_spec)

    return target_specs


def _valid_leeways(test_spec: TestSpec) -> None:
    # Set up the size bounds based on the size and leeway parameters. Have to
    # make up your mind whether to use relative xor absolute size leeways.
    num_leeway_settings_used = sum(
        0 if x is None else 1
        for x in [
            test_spec.size_leeway,
            test_spec.pos_size_leeway,
            test_spec.neg_size_leeway,
        ]
    )
    num_abs_leeway_settings_used = sum(
        0 if x is None else 1
        for x in [
            test_spec.abs_size_leeway,
            test_spec.pos_abs_size_leeway,
            test_spec.neg_abs_size_leeway,
        ]
    )
    if num_abs_leeway_settings_used > 0 and num_abs_leeway_settings_used:
        raise TypeError(
            "Relative and absolute size leeways can't both be specified for the"
            " same test."
        )


# TODO use `ValidatedTestSpec` precision type for input.
def lower_size_bound(test_spec: TestSpec, is_bloated: bool) -> int:
    size_opt = (
        test_spec.expected_size_bloated
        if is_bloated
        else test_spec.expected_size_debloated
    )
    assert size_opt is not None  # Test kind should check for this already
    size = size_opt

    abs_leeway = unwrap_or(
        fallback_list(
            test_spec.neg_abs_size_leeway,
            test_spec.abs_size_leeway,
            option_map(test_spec.neg_size_leeway, lambda t: t * size),
            option_map(test_spec.size_leeway, lambda t: t * size),
        ),
        0,
    )
    return int(size - abs_leeway)


def upper_size_bound(test_spec: TestSpec, is_bloated: bool) -> int:
    size_opt = (
        test_spec.expected_size_bloated
        if is_bloated
        else test_spec.expected_size_debloated
    )
    assert size_opt is not None  # Test kind should check for this already
    size = size_opt

    abs_leeway = unwrap_or(
        fallback_list(
            test_spec.pos_abs_size_leeway,
            test_spec.abs_size_leeway,
            option_map(test_spec.pos_size_leeway, lambda t: t * size),
            option_map(test_spec.size_leeway, lambda t: t * size),
            0,
        ),
        0,
    )
    return int(size + abs_leeway)
