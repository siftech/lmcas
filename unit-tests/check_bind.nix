# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ musl-gclang-stdenv, runCommand, tabacco-driver }:

let
  buildTest = musl-gclang-stdenv.mkDerivation {
    name = "build-bind-tests";
    src = ./.;
    buildPhase = ''
      mkdir -p $out/bin
      musl-gclang ./check_bind_to_zero_port_ipv4.c -o check_bind_to_zero_port_ipv4
      musl-gclang ./check_bind_to_zero_port_ipv6.c -o check_bind_to_zero_port_ipv6
    '';
    installPhase = ''
      install -Dt $out/bin check_bind_to_zero_port_ipv4 check_bind_to_zero_port_ipv6
    '';

  };

in runCommand "check-bind-0-rejected" {
  passthru.build = buildTest;

  nativeBuildInputs = [ tabacco-driver ];
} ''
  set -euo pipefail
  mkdir -p $out

  failed_test=false

  # ipv4 bind call check
  sed -e "s|\"binary\": \"check_bind_to_zero_port_ipv4\"|\"binary\":\"${buildTest}/bin/check_bind_to_zero_port_ipv4\"|g" \
      ${./check_bind_to_zero_port_ipv4.json} > $TMPDIR/spec1.json
  if ${tabacco-driver}/bin/tabacco $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR --no-use-neck-miner  |& tee $out/tabacco-output; then
    echo "Previous command succeeded, which is a problem because it should fail."
  else
    if grep -q "Rejecting bind call as it's an AF_INET call with port 0" $out/tabacco-output
    then
      echo "Got expected output, this command failed successfully."
    else
      echo "Error, didnt see error message in $out/tabacco-output, (did you adjust the error message and forget to adjust this test?)"
      failed_test=true
    fi
  fi

  # ipv6 bind call check
  sed -e "s|\"binary\": \"check_bind_to_zero_port_ipv6\"|\"binary\":\"${buildTest}/bin/check_bind_to_zero_port_ipv6\"|g" \
      ${./check_bind_to_zero_port_ipv6.json} > $TMPDIR/spec1.json
  if tabacco $TMPDIR/spec1.json -o $out/debloated --tmpdir $TMPDIR --no-use-neck-miner  |& tee $out/tabacco-output; then
    echo "Previous command succeeded, which is a problem because it should fail."
  else
    if grep -q "Rejecting bind call as it's an AF_INET6 call with port 0" $out/tabacco-output
    then
      echo "Got expected output, this command failed successfully."
    else
      echo "Error, didnt see error message in $out/tabacco-output, (did you adjust the error message and forget to adjust this test?)"
      failed_test=true
    fi
  fi
  if [ "$failed_test" = true ] ; then
      echo "Failed one of the tests. See output above for details."
      exit -1
  else
      echo "2/2 Tests Passed."
      exit 0
  fi
''
