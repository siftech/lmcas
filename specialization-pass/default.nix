# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ abseil-cpp, cmake, llvmPkgs, ninja, nlohmann_json, spdlog, }:

llvmPkgs.stdenv.mkDerivation {
  name = "specialization-pass";

  src = ./.;

  nativeBuildInputs = [ cmake ninja ];
  buildInputs = [ abseil-cpp llvmPkgs.llvm nlohmann_json spdlog ];
  cmakeBuildType = "RelWithDebInfo";
  dontStrip = true;
}
