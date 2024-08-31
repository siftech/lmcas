# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from setuptools import setup

setup(
    name="lmcas_metrics",
    packages=["lmcas_metrics"],
    install_requires=["lmcas_sh"],
    package_data={"lmcas_metrics": ["py.typed"]},
)
