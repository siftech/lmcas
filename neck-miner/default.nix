# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ abseil-cpp, boost165, cmake, curl, llvmPkgs, ninja, nlohmann_json, patchelf
, phasar, python39, spdlog, sqlite, time }:

llvmPkgs.stdenv.mkDerivation {
  name = "neck-miner";
  src = ./.;
  nativeBuildInputs = [
    cmake
    ninja
    patchelf
    (python39.withPackages (p: [ p.matplotlib p.pytest p.tabulate ]))
    time
  ];
  buildInputs =
    [ abseil-cpp boost165 llvmPkgs.llvm nlohmann_json phasar spdlog ];

  cmakeBuildType = "Release";

  #fixupPhase = ''
  #  patchelf --add-rpath $out/lib/phasar \
  #    $out/bin/neck $out/bin/neck-miner
  #'';
}
