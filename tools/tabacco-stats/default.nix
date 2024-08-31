# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ binutils-unwrapped, buildPythonApplication, click, lmcas-metrics, mypy
, nix-gitignore, python }:

buildPythonApplication rec {
  name = "tabacco-stats";
  src = nix-gitignore.gitignoreSourcePure [ ../../.gitignore ] ./.;

  pythonBuildInputs = [ click lmcas-metrics ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package tabacco_stats \
      --python-executable ${python}/bin/python \
      --strict
  '';
  doCheck = true;

  makeWrapperArgs = [ "--prefix PATH : ${binutils-unwrapped}/bin" ];

  meta.description =
    "A tool to show debloating metrics of a target as structured JSON.";
}
