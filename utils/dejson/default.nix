# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, multimethod, mypy, nix-gitignore, python, }:

buildPythonPackage rec {
  name = "lmcas-dejson";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  # https://github.com/python/mypy/issues/5701#issuecomment-599005932
  pythonBuildInputs = [ multimethod ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package lmcas_dejson \
      --python-executable ${pythonWithBuildInputs}/bin/python \
      --strict
  '';
  doCheck = true;

  meta.description = "Turns Python datatypes into dataclasses.";
}
