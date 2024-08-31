# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, mypy, nix-gitignore, }:

buildPythonPackage {
  name = "lmcas-sh";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package lmcas_sh \
      --strict
  '';
  doCheck = true;

  meta.description = "Replacement for sh.py that typechecks.";
}
