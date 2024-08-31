# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ llvmPkgs, musl-gclang, stdenv, }:

# Not using musl-gclang-stdenv because we don't want e.g. fixBitcodePathsPhase.
stdenv.mkDerivation {
  name = "tabacco-helpers";
  src = ./.;
  nativeBuildInputs = [ llvmPkgs.llvm musl-gclang ];
  buildPhase = ''
    for f in at_neck noop_sighandler; do
      musl-gclang -c -o $f.o $f.c
      get-bc -o $f.bc $f.o
    done
  '';
  installPhase = "llvm-link -o $out *.bc";
}
