# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-connect-test";
    src = ./.;
    # src = ./check_connect.c;
    buildPhase = ''
      mkdir -p $out/bin
      musl-gclang ./check_connect.c -o $TMPDIR/check_connect
    '';
    installPhase = ''
      mv $TMPDIR/check_connect $out/bin/check_connect
    '';

  };

in runCommand "check-connect-enoent" {
  passthru.build = buildTest;

  nativeBuildInputs = [ tabacco-driver ];
} ''
  set -euo pipefail
  mkdir -p $out

  failed_test=false

  # ipv4 bind call check
  sed -e "s|\"binary\": \"check_connect\"|\"binary\":\"${buildTest}/bin/check_connect\"|g" \
      ${./check_connect.json} > $TMPDIR/spec1.json
  if ${tabacco-driver}/bin/tabacco $TMPDIR/spec1.json -o $out/debloated -vv --tmpdir $TMPDIR --no-use-neck-miner  |& tee $out/tabacco-output; then
    if grep -q "Returning ENOENT for connect as its using address family AF_UNIX." $out/tabacco-output
    then
      echo "Got expected output from command."
    else
      echo "Error, didnt see error message in $out/tabacco-output, (did you adjust the error message and forget to adjust this test?)"
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
