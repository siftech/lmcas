# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

from setuptools import setup

setup(
    name="lmcas_test_specs",
    packages=["lmcas_test_specs"],
    install_requires=["lmcas_dejson", "lmcas_utils"],
    package_data={"lmcas_test_specs": ["py.typed"]},
)
