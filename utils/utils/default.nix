# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, multimethod, mypy, nix-gitignore, python }:

buildPythonPackage rec {
  name = "lmcas-utils";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package lmcas_utils \
      --strict
  '';
  doCheck = true;

  meta.description = "Utilities.";
}

