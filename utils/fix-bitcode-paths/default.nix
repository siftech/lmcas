# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ binutils-unwrapped, buildPythonApplication, click, lmcas-sh, mypy, python }:

buildPythonApplication rec {
  name = "fix-bitcode-paths";
  src = ./.;

  # https://github.com/python/mypy/issues/5701#issuecomment-599005932
  pythonBuildInputs = [ click lmcas-sh ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package fix_bitcode_paths \
      --python-executable ${pythonWithBuildInputs}/bin/python \
      --strict
  '';
  doCheck = true;

  makeWrapperArgs = [ "--prefix PATH : ${binutils-unwrapped}/bin" ];
}
