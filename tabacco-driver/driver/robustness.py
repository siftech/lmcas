#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import json
import random
import copy
import pprint
from pathlib import Path
from typing import Any, Optional, Callable
from dataclasses import dataclass
from . import drive, utils
from .spec import Spec, OptionConfig
from .utils import load_json_file


def perturbate_specs(
    input_config: OptionConfig, effort_constant: int, verbose: int
) -> list[tuple[list[OptionConfig], str]]:
    """
    Create a set of random specs to perturb, varying uid, gid, pid, euid, ppid between them.
    Returns the set of random specs produced.
    """

    start_spec = input_config
    syscall_mocks = start_spec.syscall_mocks
    random_spec_sets: list[tuple[list[OptionConfig], str]] = []

    # We vary specs on uid, gid, pid
    def perturbate_helper(
        gen_mock: Callable[..., dict[str, int]],
        syscall_name: str,
        field_name: str,
        defaults_list: list[int],
    ) -> list[tuple[list[OptionConfig], str]]:
        random_spec_set: list[OptionConfig] = []
        unused_defaults: list[int] = defaults_list
        # We only do the variation if they arent mocking this
        # syscall already
        if syscall_name not in start_spec.syscall_mocks:
            for _ in range(effort_constant):
                new_spec = copy.deepcopy(start_spec)
                new_mocks = copy.deepcopy(syscall_mocks)
                if len(unused_defaults) >= 1:
                    value = unused_defaults.pop()
                    generated_mock = {field_name: value}
                else:
                    generated_mock = gen_mock()
                new_mocks[syscall_name] = generated_mock
                if verbose > 2:
                    print("generated mock: {}".format(generated_mock))
                new_spec.syscall_mocks = new_mocks
                random_spec_set.append(new_spec)
            return [(random_spec_set, field_name)]
        else:
            return []

    # start by varying uid
    random_spec_sets += perturbate_helper(
        lambda: {"uid": random.randrange(1001, 2000)}, "sys_getuid", "uid", [0]
    )
    # now we vary gid
    random_spec_sets += perturbate_helper(
        lambda: {"gid": random.randrange(1001, 2000)}, "sys_getgid", "gid", [1]
    )

    # euid
    random_spec_sets += perturbate_helper(
        lambda: {"euid": random.randrange(1001, 2000)}, "sys_geteuid", "euid", [0]
    )

    # and pid
    random_spec_sets += perturbate_helper(
        lambda: {"pid": random.randrange(1001, 2000)}, "sys_getpid", "pid", [1]
    )

    # ppid
    random_spec_sets += perturbate_helper(
        lambda: {"ppid": random.randrange(1001, 2000)}, "sys_getppid", "ppid", [1]
    )

    if verbose > 2:
        print("Finished pertubating specs")
        print("made {} spec groups".format(len(random_spec_sets)))
    return random_spec_sets


def run_determinism_checker(
    tmp: Path, tape_files: list[Path], spec_path: Path, verbose: int
) -> Path:
    """
    Runs the tape determinism checker on the tape files, returning the results in a json file.
    """

    tape_files_arg = [str(tape_path) for tape_path in tape_files]
    out = tmp / "tape-checker-results.json"
    if verbose > 0:
        verbose_flag = "-" + ("v" * verbose)
        utils.run(
            "tape-determinism-checker",
            verbose_flag,
            "-o",
            str(out),
            "-s",
            spec_path,
            *tape_files_arg,
            verbose=verbose,
            check=True,
        )
    else:
        utils.run(
            "tape-determinism-checker",
            *tape_files_arg,
            "-o",
            str(out),
            "-s",
            spec_path,
            *tape_files_arg,
            verbose=verbose,
            check=True,
        )
    return out


@dataclass
class DifferenceOutputs:
    """Determinism checker differences output"""

    groups: list[list[str]]
    """A set of disjoint sets of paths, representing the different equivalence classes of tape"""

    differences: list[list[Optional[int]]]
    """Matrix representing differences. each entry with index i,j represents whether there exists a difference between set i, set j"""


def check_variation_in_specs(
    step_num: int,
    instrumented: Path,
    tmpdir: Path,
    verbose: int,
    i: int,
    spec_group: tuple[list[OptionConfig], str],
    original_spec: Spec,
    original_config: OptionConfig,
) -> tuple[int, bool]:
    tape_set = []
    """
    Produces a tape for each spec, runs the tape determinism checker on this set of tapes, 
    and determines the variations between them.
    Returns the step number and a boolean for whether there were differences found between the tapes.
    """

    pp = pprint.PrettyPrinter(indent=4)
    for j, spec in enumerate(spec_group[0]):
        if verbose > 2:
            print("running spec with syscall_mocks:\n{}".format(spec.syscall_mocks))
            print("Varying {}".format(spec_group[1]))
        debloating_spec_path = tmpdir / ("rand_spec_{}".format(i))
        with open(debloating_spec_path, "w") as f:
            json.dump(spec, f, cls=utils.DataclassJSONEncoder)
        tape_name = "tape_{}.json".format(j)
        step_num, tape = drive.produce_tape(
            step_num,
            tmpdir,
            spec,
            instrumented,
            verbose,
            tape_name=tape_name,
        )
        tape_set.append(tape)
    # now we run the tape checker and report its results.

    # create original spec type
    original: dict[str, Any] = {
        "binary": original_spec.binary,
        "args": original_config.args,
        "env": original_config.env,
        "cwd": original_config.cwd,
        "ignore_indexes": original_config.ignore_indexes,
        "syscall_mocks": copy.copy(original_config.syscall_mocks),
    }

    original_spec_file = tmpdir / f"robustness_spec_{step_num}.json"
    with original_spec_file.open("w") as file:
        json.dump(original, file, cls=utils.DataclassJSONEncoder)

    out = run_determinism_checker(tmpdir, tape_set, original_spec_file, verbose)
    output: DifferenceOutputs = load_json_file(out, DifferenceOutputs)
    number_groups = len(output.groups)
    if verbose > 2:
        print(
            "output groups is of length {}, and the difference list is:\n{}".format(
                number_groups, output.differences
            )
        )
    found_difference = number_groups > 1
    if found_difference:
        # We try to find the first difference found in the tapes.
        # Finding the minimum value, and the tape elements that correspond
        # to the difference.
        min_val = None
        min_val_left_element = None
        min_val_right_element = None
        for row_index, row in enumerate(output.differences):
            for col_index, elem in enumerate(row):
                if elem and ((min_val and elem < min_val) or min_val == None):
                    min_val = elem
                    min_val_right_element = output.groups[row_index][0]
                    min_val_left_element = output.groups[col_index][0]
        if min_val_left_element and min_val_right_element and min_val:
            # read in both tapes, find the elements that differ
            with open(min_val_right_element, "r") as f:
                right_contents = json.load(f)
                if type(right_contents) != list:
                    print(
                        "Error: tape file is not a list but is instead of type {}".format(
                            type(right_contents)
                        )
                    )
                    exit(1)
                right_element = right_contents[min_val]
            with open(min_val_left_element, "r") as f:
                left_contents = json.load(f)
                if type(left_contents) != list:
                    print(
                        "Error: tape file is not a list but is instead of type {}".format(
                            type(right_contents)
                        )
                    )
                    exit(1)
                left_element = left_contents[min_val]
            print(
                "When we varied {} and ran the program, we found a difference in execution behavior".format(
                    spec_group[1]
                )
            )
            print(
                "tape elements are different at index {}, the first has the following element".format(
                    min_val
                )
            )
            pp.pprint(left_element)
            print("and the second has the following element")
            pp.pprint(right_element)
            if "syscall" in left_element and "syscall" in right_element:
                if left_element["syscall"] == right_element["syscall"]:
                    if left_element["syscall"] in ["sys_getpid", "sys_getppid"]:
                        print(
                            "The difference in execution appears to be the result of calls to {} before the neck. "
                            "TaBaCCo does not currently support programs that store and use PID values before the neck. "
                            'For more details please look at the subsection labeled "Programs that use PID" '
                            'in the "Known Issues" section of the TaBaCCo manual.'.format(
                                left_element["syscall"]
                            )
                        )
                    else:
                        print(
                            "It appears that this difference is due to differing behavior when the syscall {} is called.".format(
                                left_element["syscall"]
                            )
                        )

                else:
                    print(
                        "It looks like in the first tape the syscall {} is called, whereas in the second the syscall {} is called".format(
                            left_element["syscall"], right_element["syscall"]
                        )
                    )

            if spec_group[1] in [
                "uid",
                "euid",
                "gid",
            ]:
                print(
                    "For this syscall, you can mock the syscall in the following way "
                    "to freeze it to a particular value (1000, in this case, but other values are acceptable), "
                    "and remove this potential source of variation in execution."
                )
                print(
                    '"syscall_mocks": {{\n  "sys_get{}": {{\n    "{}": 1000\n  }}\n}}'.format(
                        spec_group[1], spec_group[1]
                    )
                )

    return step_num, found_difference


def run_multi_tape_checker(
    step_num: int,
    instrumented: Path,
    effort_constant: int,
    debloating_spec: Spec,
    tmpdir: Path,
    verbose: int,
) -> tuple[int, bool]:
    """
    Runs the tape determinism checker for a number of randomly perturbed specs and
    determines whether there are differences between the produced tapes.
    Returns the step number and a boolean on whether differences between the tapes were detected.
    """

    utils.step_done(step_num, f"Run tape robustness checker.")
    step_num += 1

    variation_check_output = []
    for option_config in debloating_spec.configs:
        rand_specs = perturbate_specs(option_config, effort_constant, verbose)
        for i, spec_group in enumerate(rand_specs):
            tmpdir_local = tmpdir / str(i)
            tmpdir_local.mkdir(parents=True, exist_ok=True)
            step_num, found_variation = check_variation_in_specs(
                step_num,
                instrumented,
                tmpdir_local,
                verbose,
                i,
                spec_group,
                debloating_spec,
                option_config,
            )
            if found_variation:
                variation_check_output.append(spec_group[1])
        if len(variation_check_output) > 0:
            print(
                "When we varied {} we saw a difference in program behavior, and thus are stopping tabacco until this is resolved.".format(
                    ",".join(variation_check_output)
                )
            )
            print(
                "Above this should be information about the variation and how it can be handled (if it can be)."
            )
            return step_num, True
    return step_num, False
