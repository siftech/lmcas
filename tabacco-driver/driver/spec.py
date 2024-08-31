# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional
from .neck_miner import NeckMinerConfig


@dataclass
class NeckLocation:
    """A program point that should be treated as a neck."""

    basic_block_annotation_id: str
    """
    The index of the basic block, as assigned by the annotation pass. This is
    represented as a string to avoid losing low bits once it gets too large.
    """

    insn_index: int
    """
    The index of the instruction that immediately follows the indicated neck.
    """


@dataclass
class OptionConfig:
    """A single configuration of the output binary."""

    args: list[str]
    """Args to specialize on. This should include argv[0]`."""

    env: dict[str, str]
    """ Environment variables the binary should be run with.
    Variables like `PWD` or `USER` are not automatically
    included and should be specified if they are required. 
    """

    cwd: Path
    """The directory the binary should be run in."""

    ignore_indexes: Optional[list[int]] = None
    """A list of indexes to ignore when using the robustness checker 
    to look for sources of nondeterministic behavior"""

    syscall_mocks: dict[str, Any] = field(default_factory=dict)
    """
    Configuration for mocking syscalls.
    """


@dataclass
class Spec:
    """A specification for the configuration of a single run of debloating"""

    binary: Path
    """A path to the binary."""

    configs: list[OptionConfig]
    """The configurations of the output binary to allow."""

    use_neck_miner: bool = True
    """A flag on whether to use neck-miner"""

    use_guiness: bool = False
    """A flag on whether to use guiness"""

    replace_sighup: bool = False
    """A flag on whether to replace sighup"""

    neck_miner_config: Optional[NeckMinerConfig] = None
    """Configuration data for neck-miner"""

    extra_neck_locations: list[NeckLocation] = field(default_factory=list)
    """
    Additional neck locations to use.
    """
