# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv }:

musl-gclang-stdenv.mkDerivation {
  name = "tabacco-cps";
  src = ./.;
  makeFlags = [ "PREFIX=$(out)" ];
  dontStrip = true;

  passthru.bin-paths = {
    cp1 = "bin/cp1";
    cp2 = "bin/cp2";
    cp3 = "bin/cp3";
    cp4 = "bin/cp4";
    cp5 = "bin/cp5";
    cp6 = "bin/cp6";
    cp7 = "bin/cp7";
    cp8 = "bin/cp8";
    cp9 = "bin/cp9";
    cp_sighup_after_neck = "bin/cp_sighup_after_neck";
    cp_sighup_before_neck = "bin/cp_sighup_before_neck";
    cp_umask = "bin/cp_umask";
    cp_fcntl_getfd = "bin/cp_fcntl_getfd";
  };
  passthru.specs = import ../../test/tabacco-targets/tabacco-cps/specs;
}
