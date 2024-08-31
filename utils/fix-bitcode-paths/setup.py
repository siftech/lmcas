# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from setuptools import setup

setup(
    name="fix-bitcode-paths",
    packages=["fix_bitcode_paths"],
    install_requires=["click", "lmcas_sh"],
    entry_points={"console_scripts": ["fix-bitcode-paths = fix_bitcode_paths:main"]},
)
