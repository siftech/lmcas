# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver, writeText, }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-sigaction-test";
    src = ./.;
    buildPhase = "musl-gclang main.c -o check-sigaction";
    installPhase = "install -Dt $out/bin check-sigaction";
  };

  spec = {
    binary = "${buildTest}/bin/check-sigaction";
    configs = [{
      args = [ "check-sigaction" ];
      env = { };
      cwd = "/";
    }];
  };
  specFile = writeText "check-sigaction.json" (builtins.toJSON spec);
  expected = writeText "expected.txt" ''
    2
  '';

in runCommand "sigaction-test" {
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
