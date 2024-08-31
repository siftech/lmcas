# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from dataclasses import dataclass
from pathlib import Path


@dataclass
class NeckMinerTaintFunction:
    """Neck-miner function configratuon"""

    name: str
    """Name of the function"""

    params: dict[str, list[int]]
    """Function parameters"""


@dataclass
class NeckMinerTaintConfig:
    """Configuation of neck-miner taint analysis"""

    name: str
    """Name of the configuration"""

    version: float
    """Version of the configuration"""

    functions: list[NeckMinerTaintFunction]
    """List of functions to mine"""


@dataclass
class NeckMinerConfig:
    """Configuation of neck-miner"""

    function_local_points_to_info_wo_globals: bool
    """function-local-points-to-info-wo-globals flag"""

    use_simplified_dfa: bool
    """use-simplified-dfa flag"""

    taint_config: NeckMinerTaintConfig
    """Taint analysis configuration"""
