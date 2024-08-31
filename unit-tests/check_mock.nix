# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-mock-test";
    src = ./.;
    # src = ./check_mock.c;
    buildPhase = ''
      mkdir -p $out/bin
      musl-gclang ./check_mock.c -o $TMPDIR/check_mock
    '';
    installPhase = ''
      mv $TMPDIR/check_mock $out/bin/check_mock
    '';

  };

in runCommand "check-mock-correct" {
  passthru.build = buildTest;

  nativeBuildInputs = [ tabacco-driver ];
} ''
  set -euo pipefail
  mkdir -p $out

  failed_test=false

  sed -e "s|\"binary\": \"check_mock\"|\"binary\":\"${buildTest}/bin/check_mock\"|g" \
      ${./check_mock.json} > $TMPDIR/spec1.json
  if ${tabacco-driver}/bin/tabacco $TMPDIR/spec1.json -o $out/debloated -vv --tmpdir $TMPDIR --no-use-neck-miner  |& tee $out/tabacco-output; then
    if grep -q "uid=1000, euid=1001, gid=1002, pid=1003, ppid=1004" $out/tabacco-output
    then
      echo "Got expected output from command."
    else
      echo "Error, didnt see error message in $out/tabacco-output, (did you adjust the error message and forget to adjust this test?)"
      cp $out/tabacco-output $TMPDIR/tabacco-output
      failed_test=true
    fi
  else
    echo "previous command failed, see output above for details."
    failed_test=true
  fi

  if [ "$failed_test" = true ] ; then
      echo "Failed one of the tests. See output above for details."
      exit -1
  else
      echo "1/1 Tests passed."
      exit 0
  fi
''
