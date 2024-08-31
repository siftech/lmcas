# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ buildPythonApplication, mypy }:

buildPythonApplication rec {
  name = "make-external-list";
  src = ./.;

  checkInputs = [ mypy ];

  checkPhase = ''
    mypy --no-color-output \
      --package make_external_list \
      --strict
  '';
  doCheck = true;

  meta = {
    description =
      "A program to list symbols that need to be provided to link with some objects.";
    mainProgram = "make_external_list";
  };
}
