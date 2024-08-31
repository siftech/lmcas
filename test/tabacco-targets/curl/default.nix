# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ fetchurl, lzip, musl-gclang-stdenv, lib }:

musl-gclang-stdenv.mkDerivation rec {
  pname = "curl";
  version = "7.83.1";

  src = fetchurl {
    urls = [
      "https://curl.haxx.se/download/${pname}-${version}.tar.bz2"
      "https://github.com/curl/curl/releases/download/${pname}-${version}/${pname}-${version}.tar.bz2"
    ];
    sha256 = "sha256-9Tmjb7RKgmDsXZd+Tg290u7intkPztqpvDyfeKETv/A=";
  };
  configureFlags = [ "--without-ssl" "--disable-shared" "--enable-static" ];
  meta.mainProgram = "curl";

  passthru.bin-paths = { curl = "bin/curl"; };
  passthru.specs = import ./specs;
}
