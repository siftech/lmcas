# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver, writeText, }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-term-size-test";
    src = ./.;
    buildPhase = "musl-gclang check_term_size.c -o check_term_size";
    installPhase = "install -Dt $out/bin check_term_size";

  };

  spec = {
    binary = "${buildTest}/bin/check_term_size";
    configs = [{
      args = [ "check_term_size" ];
      env = { };
      cwd = "/";
      syscall_mocks.sys_ioctl = {
        terminal_dimensions = {
          row = 25;
          col = 80;
        };
        unsafe_allow_tiocgwinsz = true;
      };
    }];
  };
  specFile = writeText "check_term_size.json" (builtins.toJSON spec);
  expected = writeText "expected.txt" ''
    0x0
  '';

in runCommand "term-size-test" {
  passthru.build = buildTest;

  nativeBuildInputs = [ tabacco-driver ];
} ''
  set -x

  tabacco ${specFile} -o debloated -vv --no-use-neck-miner
  ./debloated |& tee log
  diff log ${expected}

  mkdir $out
  set +x
''
