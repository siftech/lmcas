# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ llvmPkgs, stdenv }:

stdenv.mkDerivation {
  name = "instrumentation-runtime";
  src = ./.;
  nativeBuildInputs = [ llvmPkgs.clangUseLLVM ];
  installPhase = "install lmcas_instrumentation_runtime.bc $out";
}
