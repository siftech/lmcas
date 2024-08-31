# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from setuptools import setup

setup(
    name="tabacco-driver",
    packages=["driver"],
    install_requires=["click", "lmcas_sh", "lmcas_dejson"],
    entry_points={"console_scripts": ["tabacco = driver.main:main"]},
)
