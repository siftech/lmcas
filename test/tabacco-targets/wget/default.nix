# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ fetchurl, lzip, musl-gclang-stdenv }:

musl-gclang-stdenv.mkDerivation rec {
  pname = "wget";
  version = "1.21.3";

  src = fetchurl {
    url = "mirror://gnu/wget/${pname}-${version}.tar.lz";
    sha256 = "sha256-29L7XkcUnUdS0Oqg2saMxJzyDUbfT44yb/yPGLKvTqU=";
  };
  nativeBuildInputs = [
    # Needed to extract sources from tarball.
    lzip
  ];

  configureFlags = [ "--without-ssl" "--without-zlib" ];
  meta.mainProgram = "wget";

  passthru.bin-paths = { wget = "bin/wget"; };
  passthru.specs = import ./specs;
}
