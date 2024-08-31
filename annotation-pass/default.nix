# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ llvmPkgs, cmake, ninja, spdlog, nix-gitignore, }:

llvmPkgs.stdenv.mkDerivation {
  name = "annotation-pass";

  src = nix-gitignore.gitignoreSource [ ../.gitignore ./.gitignore ] ./.;

  nativeBuildInputs = [ cmake ninja ];
  buildInputs = [ llvmPkgs.llvm spdlog ];
  cmakeBuildType = "RelWithDebInfo";
  dontStrip = true;
}
