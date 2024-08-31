# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import dataclasses
import datetime
import json
from lmcas_dejson import load_json
from lmcas_sh import sh
from os import getenv
from os.path import basename
from pathlib import Path
import sys
from time import time
from typing import Any, Optional, Type, TypeVar, Union

CYAN = "\033[94m"
GRAY = "\033[90m"
GREEN = "\033[92m"
WARNING = "\033[93m"
WHITE = "\033[97m"

BOLD = "\033[1m"
NORMAL = "\033[0m"


def getenv_or_die(env_var_key: str) -> str:
    """Throws an exception if getenv returns None"""

    val_of_key = getenv(env_var_key)
    if val_of_key is None:
        raise Exception(
            "Value of environment variable key " + env_var_key + " does not exist"
        )
    return val_of_key


def step(step_color: str, step_num: int, step_msg: str) -> None:
    """Prints step string with specified color and formatting"""

    print(step_color + "Step " + str(step_num) + ": " + step_msg + WHITE)


def step_done(step_num: int, step_msg: str) -> None:
    """Prints step number and message in green"""

    step(GREEN, step_num, step_msg)


def step_doing(step_num: int, step_msg: str) -> None:
    """Prints step number and message in cyan"""

    step(CYAN, step_num, step_msg)


def step_later(step_num: int, step_msg: str) -> None:
    """Prints step number and message in gray"""

    step(GRAY, step_num, step_msg)


def run(
    command: str,
    *args: Union[str, Path],
    verbose: int,
    cwd: Optional[Union[str, Path]] = None,
    check: bool = True,
    libsegfault: bool = False
) -> None:
    """Run shell command with given args
    Add trace level to the command env
    Time and print elapsed time for running command
    Print the output of running the command
    """

    LD_PRELOAD = []
    if libsegfault:
        LD_PRELOAD.append(Path(getenv_or_die("LIBSEGFAULT")) / "lib/libsegfault.so")

    start_time = time()
    spdlog_level = ["error", "warn", "info", "debug", "trace"][verbose]
    sh[command](
        *args,
        cwd=cwd,
        env={
            "LD_PRELOAD": ":".join(str(path) for path in LD_PRELOAD),
            "SPDLOG_LEVEL": spdlog_level,
        },
        check=check
    )
    end_time = time()
    elapsed_time = str(datetime.timedelta(seconds=(end_time - start_time)))

    # Print output of running the command
    base = basename(command)
    print(BOLD + base + NORMAL + "\t\t" + "[" + elapsed_time + "]")
    sys.stdout.flush()


def extract_command_output(command: str, *args: Union[str, Path]) -> str:
    """Run given command and return stdout as string"""

    return sh.with_stdout[command](*args).strip()


class DataclassJSONEncoder(json.JSONEncoder):
    def default(self, o: Any) -> Any:
        if dataclasses.is_dataclass(o):
            return dataclasses.asdict(o)
        elif isinstance(o, Path):
            return str(o)
        return super().default(o)


T = TypeVar("T")


def load_json_file(path: Path, ty: Type[T] = Any) -> T:  # type: ignore
    with path.open() as f:
        return load_json(ty, f)
