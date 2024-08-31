# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonPackage, mypy, nix-gitignore, python, lmcas-sh }:

buildPythonPackage rec {
  name = "lmcas-metrics";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  pythonBuildInputs = [ lmcas-sh ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package lmcas_metrics \
      --python-executable ${pythonWithBuildInputs}/bin/python \
      --strict
  '';
  doCheck = true;

  meta.description = "Metrics.";
}

