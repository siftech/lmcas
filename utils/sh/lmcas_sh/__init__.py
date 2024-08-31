# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

import logging
import os
from pathlib import Path
import shlex
from subprocess import check_call, check_output, call, PIPE
import subprocess
from typing import Callable, Generic, Optional, TypeVar, Union

T = TypeVar("T")


class _ShClass(Generic[T]):
    """
    A special object for running subcommands. This is like python-sh, but
    far simpler and more friendly to static typechecking.
    """

    def __init__(
        self,
        run: Callable[[list[str], Optional[Path], Optional[dict[str, str]], bool], T],
    ) -> None:
        self._run = run

    @property
    def with_stdout(self) -> "_ShClass[str]":
        def run(
            argv: list[str],
            cwd: Optional[Path],
            env: Optional[dict[str, str]],
            check: bool,
        ) -> str:
            if check:
                return check_output(argv, cwd=cwd, env=env, encoding="utf-8")
            else:
                return subprocess.run(
                    argv,
                    stdout=PIPE,
                    check=False,
                ).stdout.decode("utf-8")

        return _ShClass(run)

    @property
    def with_stdout_bytes(self) -> "_ShClass[bytes]":
        def run(
            argv: list[str],
            cwd: Optional[Path],
            env: Optional[dict[str, str]],
            check: bool,
        ) -> bytes:
            if check:
                return check_output(argv, cwd=cwd, env=env)
            else:
                return subprocess.run(
                    argv,
                    stdout=PIPE,
                    check=False,
                ).stdout

        return _ShClass(run)

    def __getattr__(self, key: str) -> Callable[..., T]:
        return self[key]

    def __getitem__(self, key: str) -> Callable[..., T]:
        def run(
            *args: Union[Path, str],
            cwd: Optional[Union[Path, str]] = None,
            env: Optional[dict[str, str]] = None,
            check: bool = True
        ) -> T:
            argv = [key] + [str(arg) for arg in args]
            logging.getLogger("lmcas_sh.sh").debug(shlex.join(argv))

            if cwd is not None:
                cwd = Path(cwd)

            if env is not None:
                new_env = os.environ.copy()
                new_env.update(env)
                env = new_env

            return self._run(argv, cwd, env, check)

        return run


def _default_run(
    argv: list[str],
    cwd: Optional[Path],
    env: Optional[dict[str, str]],
    check: bool,
) -> None:
    """
    A wrapper for subprocess.check_call that returns None instead of 0.
    """
    if check:
        check_call(argv, cwd=cwd, env=env)
    else:
        call(argv, cwd=cwd, env=env)
    return


sh: _ShClass[None] = _ShClass(_default_run)
