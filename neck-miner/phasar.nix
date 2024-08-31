# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ boost165, cmake, curl, fetchFromGitHub, llvmPkgs, ninja, nlohmann_json
, patchelf, sqlite }:

llvmPkgs.stdenv.mkDerivation {
  name = "phasar";
  src = fetchFromGitHub {
    owner = "sift-jladwig";
    repo = "phasar";
    rev = "e0470e704775a1413bc55dab2c6f13c2230a0ce1";
    fetchSubmodules = true;
    hash = "sha256-1yeik755dCjHKP1ie1d0E+xz5OVr0RVE9zDIpTxZ5wQ=";
  };
  nativeBuildInputs = [ cmake ninja ];
  buildInputs = [ boost165 curl llvmPkgs.libclang llvmPkgs.llvm sqlite ];

  patches = [ ./phasar-llvm-12.patch ];

  # This avoids Phasar installing config files into /home...
  preConfigure = ''
    export HOME="$TMPDIR"
  '';
  cmakeBuildType = "Release";
}
