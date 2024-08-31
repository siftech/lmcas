# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ binutils, bloaty, buildPythonApplication, click, gllvm, lddcollect, llvmPkgs
, lmcas-dejson, lmcas-sh, make-external-list, musl, mypy, nix-gitignore, python
, runCommand,

# TaBaCCo Passes
annotation-pass, instrumentation-pass, instrumentation-libc
, instrumentation-parent, libsegfault, neck-miner, sighup-pass
, specialization-pass, tabacco-helpers }:

buildPythonApplication rec {
  name = "tabacco-driver";
  src = nix-gitignore.gitignoreSourcePure [ ../.gitignore ] ./.;

  pythonBuildInputs = [ click lddcollect lmcas-dejson lmcas-sh ];
  pythonWithBuildInputs = python.withPackages (_: pythonBuildInputs);
  propagatedBuildInputs = pythonBuildInputs;
  checkInputs = [ mypy ];

  makeWrapperArgs = [
    "--prefix PATH : ${binutils}/bin"
    "--prefix PATH : ${bloaty}/bin"
    "--prefix PATH : ${llvmPkgs.clang}/bin"
    "--prefix PATH : ${gllvm}/bin"
    "--prefix PATH : ${instrumentation-parent}/bin"
    "--prefix PATH : ${llvmPkgs.llvm}/bin"
    "--prefix PATH : ${make-external-list}/bin"
    "--prefix PATH : ${neck-miner}/bin"

    "--set ANNOTATION_PASS_PATH ${annotation-pass}/lib/libLmcasAnnotationPass.so"
    "--set HANDLE_SIGHUP_PASS_PATH ${sighup-pass}/lib/libLmcasSighupPass.so"
    "--set TABACCO_HELPERS_BITCODE ${tabacco-helpers}"
    "--set INSTRUMENTATION_LIBC_PATH ${instrumentation-libc.bitcode}"
    "--set INSTRUMENTATION_PASS_PATH ${instrumentation-pass}/lib/libLmcasInstrumentationPass.so"
    "--set LIBSEGFAULT ${libsegfault}"
    "--set MUSL_PATH ${musl}/lib"
    "--set SPECIALIZATION_PASS_PATH ${specialization-pass}/lib/libLmcasSpecializationPass.so"
  ];

  checkPhase = ''
    mypy --no-color-output \
      --package driver \
      --python-executable ${pythonWithBuildInputs}/bin/python \
      --strict
  '';
  doCheck = true;

  meta = {
    description =
      "A driver for TaBaCCo to specialize and debloat a given program";
    mainProgram = "tabacco";
  };
}
