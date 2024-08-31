# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ fetchurl, lzip, musl-gclang-stdenv, }:

musl-gclang-stdenv.mkDerivation rec {
  pname = "dropbear";
  version = "2020.81";

  src = fetchurl {
    url = "https://mirror.dropbear.nl/mirror/${pname}-${version}.tar.bz2";
    sha256 = "sha256-SCNdELN3ddvaWTQawMSyObgq1jGMMVaLmFcwx4iqxTs=";
  };
  nativeBuildInputs = [
    # Needed to extract sources from tarball.
    lzip
  ];

  configureFlags = [ "--disable-zlib" ];
  meta.mainProgram = "dropbear";

  passthru.bin-paths = { dropbear = "bin/dropbear"; };
  passthru.specs = import ./specs;
}
