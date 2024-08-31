# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver }:

let
  build = musl-gclang-stdenv.mkDerivation {
    name = "neck-locations-json";

    src = ./.;

    buildPhase = "musl-gclang -o main main.c";
    checkPhase = ''
      test "$(./main |& cat)" = "Error: invalid mode"
      test "$(./main -h |& cat)" = "Hello, world!"
      test "$(./main -l |& cat)" = "$(ls)"
    '';
    installPhase = "install -Dt $out/bin main";

    doCheck = true;
  };

in runCommand "${build.name}-unit-test" {
  inherit build;
  nativeBuildInputs = [ tabacco-driver ];
} ''
  cp $build/bin/main input-binary
  tabacco -o debloated    --tmpdir    debloated-tmp    ${./spec.json}
  tabacco -o nondebloated --tmpdir nondebloated-tmp -N ${./spec.json}

  set -x
  test "$(./debloated |& cat)" = "USAGE: main -h [ARGS...]"
  test "$(./debloated -h |& cat)" = "Hello, world!"
  test "$(./debloated -h -h |& cat)" = "Hello, world!"
  test "$(./debloated -h -l |& cat)" = "Hello, world!"

  test "$(./nondebloated |& cat)" = "Error: invalid mode"
  test "$(./nondebloated -h |& cat)" = "Hello, world!"
  test "$(./nondebloated -l |& cat)" = "$(ls)"
  set +x

  install -Dt $out/bin debloated nondebloated
''
