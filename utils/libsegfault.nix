# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ boost, fetchFromGitHub, lib, libbacktrace, meson, ninja, stdenv }:

stdenv.mkDerivation {
  pname = "libsegfault";
  version = "unstable-2022-11-12";

  src = fetchFromGitHub {
    owner = "jonathanpoelen";
    repo = "libsegfault";
    rev = "8bca5964613695bf829c96f7a3a14dbd8304fe1f";
    hash = "sha256-vKtY6ZEkyK2K+BzJCSo30f9MpERpPlUnarFIlvJ1Giw=";
  };

  nativeBuildInputs = [ meson ninja ];
  buildInputs = [ boost libbacktrace ];

  meta = {
    description = "Implementation of libSegFault.so with Boost.stracktrace";
    homepage = "https://github.com/jonathanpoelen/libsegfault";
    license = lib.licenses.asl20;
  };
}
