# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, mypy, nix-gitignore, python, lmcas-dejson, lmcas-utils
, toml, types-toml }:

buildPythonPackage rec {
  name = "lmcas-test-specs";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  pythonBuildInputs = [ lmcas-dejson lmcas-utils toml types-toml ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package lmcas_test_specs \
      --python-executable ${pythonWithBuildInputs}/bin/python \
      --strict
  '';
  doCheck = true;

  meta.description = "Data type definitions for testing.";
}
