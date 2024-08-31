# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ gdb, musl-gclang-stdenv, runCommand, tabacco-driver, writeText }:

let
  build = musl-gclang-stdenv.mkDerivation {
    name = "build-check-preneck-noexec";
    src = ./.;
    buildPhase = "musl-gclang check_preneck_noexec.c -o check_preneck_noexec";
    installPhase = "install -Dt $out/bin check_preneck_noexec";
  };

  spec = {
    binary = "${build}/bin/check_preneck_noexec";
    configs = [{
      args = [ "check_preneck_noexec" "-h" ];
      env = {
        CWD = "/";
        PATH = "/bin";
      };
      cwd = "/";
    }];
  };
  specFile = writeText "check_preneck_noexec.json" (builtins.toJSON spec);

  debloat = musl-gclang-stdenv.mkDerivation {
    name = "debloat-check-preneck-noexec";
    nativeBuildInputs = [ tabacco-driver ];
    unpackPhase = "true";
    buildPhase = ''
      tabacco ${specFile} \
        -O0 \
        -o check_preneck_noexec \
        --no-strip-debug
    '';
    installPhase = "install -Dt $out/bin check_preneck_noexec";
  };

in runCommand "check-preneck-noexec" {
  passthru = { inherit build debloat; };
  nativeBuildInputs = [ gdb ];
} ''
  gdb ${debloat}/bin/check_preneck_noexec \
    -ex "source ${./check_preneck_noexec.py}" \
    -ex "lmcas_test $out"
''
