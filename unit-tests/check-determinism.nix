# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver, instrumentation-parent, jq }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-check-determinism-tests";
    src = ./.;
    buildPhase = ''
      mkdir -p $out/bin
      musl-gclang ./check-determinism.c -o check-determinism
    '';
    installPhase = ''
      install -Dt $out/bin check-determinism
    '';

  };

in runCommand "check-determinism" {
  passthru.build = buildTest;

  nativeBuildInputs = [ tabacco-driver instrumentation-parent jq ];
} ''
  set -euo pipefail
  mkdir -p $out

  failed_test=false

  # determinism check
  sed -e "s|\"binary\": \"check-determinism\"|\"binary\":\"${buildTest}/bin/check-determinism\"|g" \
      ${./check-determinism.json} > $TMPDIR/spec1.json

  # run tabacco five times into five different tapes
  mkdir $TMPDIR/1/
  mkdir $TMPDIR/2/
  mkdir $TMPDIR/3/
  mkdir $TMPDIR/4/
  mkdir $TMPDIR/5/

  # don't use robustness checks because we're just interested in generating tapes here.
  tabacco --no-use-neck-miner --no-robustness-checks $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR/1/
  tabacco --no-use-neck-miner --no-robustness-checks $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR/2/
  tabacco --no-use-neck-miner --no-robustness-checks $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR/3/
  tabacco --no-use-neck-miner --no-robustness-checks $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR/4/
  tabacco --no-use-neck-miner --no-robustness-checks $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR/5/

  install -Dt $out/tape1.json $TMPDIR/1/tape_8.json
  install -Dt $out/tape2.json $TMPDIR/2/tape_8.json
  install -Dt $out/tape3.json $TMPDIR/3/tape_8.json
  install -Dt $out/tape4.json $TMPDIR/4/tape_8.json
  install -Dt $out/tape5.json $TMPDIR/5/tape_8.json


  # print out the result of the determinism-check
  if tape-determinism-checker -o $TMPDIR/out.json $TMPDIR/1/tape_8.json \
      $TMPDIR/2/tape_8.json \
      $TMPDIR/3/tape_8.json \
      $TMPDIR/4/tape_8.json \
      $TMPDIR/5/tape_8.json; then
    echo "Got expected output, this command failed successfully."
    if jq '.groups | length' $TMPDIR/out.json | grep -q '5'; then
      echo "length matched"
    else
      echo "output did not match in length, printed below"
      printf "\n"
      cat $TMPDIR/out.json
      printf "\n"
      failed_test=true
    fi
  else
    echo "Call to tape-determinism-checker failed."
    failed_test=true
  fi

  # check that running it on one tape produces success and a list of length 1.
  if tape-determinism-checker $TMPDIR/1/tape_8.json -o $TMPDIR/out_one.json; then
    echo "Previous command succeeded"
    if jq '.groups | length' $TMPDIR/out_one.json | grep -q '1'; then
      echo "length matched"
    else
      echo "output did not match in length, printed below"
      printf "\n"
      cat $TMPDIR/out_one.json
      printf "\n"
      failed_test=true
    fi
  else
    echo "Previous command has nonzero exit code, which shouldn't be occurring."
    failed_test=true
  fi

  # check that running it on one tape produces success and a list of length 1.
  if tape-determinism-checker -o $TMPDIR/out_zero.json; then
    echo "Previous command succeeded"
    if jq '.groups | length' $TMPDIR/out_zero.json | grep -q '0'; then
      echo "length matched"
    else
      echo "output did not match in length, printed below"
      printf "\n"
      cat $TMPDIR/out_zero.json
      printf "\n"
      failed_test=true
    fi
  else
    echo "Previous command has nonzero exit code, which shouldn't be occurring."
    failed_test=true
  fi


  if [ "$failed_test" = true ] ; then
      echo "Failed one of the tests. See output above for details."
      exit 1
  else
      echo "3/3 Tests Passed."
      exit 0
  fi
''
