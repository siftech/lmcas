import sys

# Hack to add python modules to module search path
sys.path.append("/test/utils/utils")
sys.path.append("/test/utils/test-specs")
sys.path.append("/test/utils/dejson")

import inspect

from lmcas_utils import unwrap_or
from lmcas_test_specs import TestSpec
import lmcas_test_specs
from os import environ
from pathlib import Path
import utils
import subprocess
import re
import pytest
import functools
from typing import Callable, Optional


def make_tests(target_name: str, toml: Optional[object] = None):
    """
    Reads a TOML file, and creates tests from it. This function must be called
    from the top level of a file, not from another function.

    If a TOML file is used, it can be found in the same directory as the file
    that calls this function, and has the name `f"{name}.toml"`. It should be a
    mapping from test target names to :class:`~table_tests.table_tests.TargetSpec`
    values.

    :param target_name: The name of the target we are testing
    :param input_binary_dir: The directory both bloated and debloated binaries
            are found.
    :param toml: An optional toml object (used for target tests that do not rely
            on a toml file for test specifications).
    """

    input_binary_dir = Path("/test/") / target_name / "target-binaries"

    # Get the caller
    caller = inspect.stack()[1]
    if caller.function != "<module>":
        raise Exception("make_tests must be called at the top level")
    caller_filename = Path(caller.filename)
    caller_globals = caller.frame.f_globals

    name = caller_filename.stem

    # Load the target specifications
    if toml is not None:
        targets = lmcas_test_specs.parse_targets_file_from_value(toml)
    else:
        targets = lmcas_test_specs.parse_targets_file(
            caller_filename.parent / f"{name}.toml"
        )

    # We keep the using_binary functions (see below) around, so they can be
    # given to custom tests.
    using_binary_functions = {}

    for target_spec_name, target_spec in targets.items():

        # Because we close over the lexical environment in using_binary below,
        # we want to create a new lexical scope. This avoids the closure
        # capturing the variables' location in the environment rather than
        # their value (by not mutating the same environment each loop, but
        # rather creating a new one).
        def inner(target_spec_name, target_spec):
            bin_name = target_spec.bin_name

            # Select the specified binary for testing
            def using_binary(is_bloated: bool) -> Path:
                bloated = input_binary_dir.joinpath(f"{bin_name}/bloated/{bin_name}")
                debloated = input_binary_dir.joinpath(
                    f"{bin_name}/debloated/{bin_name}"
                )

                if is_bloated:
                    return bloated
                return debloated

            # Save the using_binary function so it can be given to custom
            # tests
            using_binary_functions[target_spec_name] = using_binary

            target_tests = unwrap_or(target_spec.tests, {})
            for test_spec_name, test_spec in target_tests.items():
                spec_test_name = f"test_{target_spec_name}_{test_spec_name}"
                spec_test_name = utils.choose_nonconflicting_name(
                    caller_globals, spec_test_name
                )

                caller_globals[spec_test_name] = make_test(
                    test_spec, functools.partial(using_binary, is_bloated=False)
                )

        inner(target_spec_name, target_spec)

        # We also inject a decorator into the calling scope that lets custom tests
        # call our using_binary functions.
        def with_use_binary(argument_name, target_spec_name):
            using_binary = using_binary_functions[target_spec_name]

            def decorator(func):
                wrapped = functools.partial(func, **{argument_name: using_binary})
                # A bit of a hack here is necessary because of PyTest... PyTest
                # only considers functions testable if they have names, so it has a
                # name to display. functools.partial, on the other hand, doesn't
                # pass a name through, since it intends to work on all callables,
                # not just "real" defined-by-def functions.
                wrapped.__name__ = func.__name__
                return wrapped

            return decorator

        caller_globals["with_use_binary"] = with_use_binary


def make_test(
    test_spec: TestSpec, using_binary: Callable[[], Path]
) -> Callable[[], None]:

    # Create the "core" of the test. This should be the part that depends on
    # the test kind, rather than anything generic.
    if test_spec.kind == "simple-run":
        # Run the binary with the specified args,
        # feed it `stdin`, and see if the result of stdout is exactly as
        # written in the spec.

        def test_core(binary):
            env = dict(environ)
            env.update(test_spec.env)
            argv0 = unwrap_or(test_spec.argv0, binary)
            args_to_debloated = [argv0] + list(test_spec.args)

            proc = subprocess.run(
                args=args_to_debloated,
                env=env,
                input=test_spec.stdin,
                capture_output=True,
                encoding="utf-8",
            )
            # These only get shown if the test fails
            print(f"proc.args = {proc.args!r}")
            print(f"test_spec.expected_status = {test_spec.expected_status!r}")
            print(f"proc.returncode = {proc.returncode!r}")
            print(f"test_spec.expected_stdout = {test_spec.expected_stdout!r}")
            print(f"proc.stdout = {proc.stdout!r}")
            print(f"test_spec.expected_stderr = {test_spec.expected_stderr!r}")
            print(f"proc.stderr = {proc.stderr!r}")

            if test_spec.expected_status is not False:
                assert proc.returncode == test_spec.expected_status
            if test_spec.expected_stdout is not False:
                if test_spec.is_stdout_regex:
                    assert re.fullmatch(test_spec.expected_stdout, proc.stdout)
                else:
                    assert proc.stdout == test_spec.expected_stdout
            if test_spec.expected_stderr is not False:
                if test_spec.is_stderr_regex:
                    assert re.fullmatch(test_spec.expected_stderr, proc.stderr)
                else:
                    assert proc.stderr == test_spec.expected_stderr

    else:
        raise TypeError(f"Invalid test kind {test_spec.kind!r}")

    def test():
        binary = using_binary()
        should_pass = test_spec.should_pass_debloated
        if should_pass:
            test_core(binary)
        else:
            with pytest.raises(AssertionError):
                test_core(binary)

    # Add any requested fixtures.
    fixtures = unwrap_or(test_spec.fixtures, [])
    test = pytest.mark.usefixtures(*fixtures)(test)

    if test_spec.broken is not None:
        test = pytest.mark.broken(test)
    test = pytest.mark.tabacco(test)

    return test
