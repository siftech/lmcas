# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv }:

musl-gclang-stdenv.mkDerivation {
  name = "mongoose";
  src = ./.;
  makeFlags = [ "PREFIX=$(out)" ];

  passthru.bin-paths = { switched-server = "bin/switched-server"; };
  passthru.specs = import ../../test/tabacco-targets/mongoose/specs;
}
