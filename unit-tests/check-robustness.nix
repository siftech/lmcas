# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

{ runCommand, tabacco-driver, wget, ... }:

runCommand "check-robustness" { nativeBuildInputs = [ tabacco-driver ]; } ''
  set -euo pipefail
  mkdir -p $out

  failed_test=false

  sed -e "s|\"binary\": \"/root/wget-1.21.3/src/wget\"|\"binary\":\"${wget}/bin/wget\"|g" \
      ${../deliv/wget/spec.json} > $TMPDIR/spec.json

  # tabacco call should fail to debloat
  if tabacco $TMPDIR/spec.json -o $out/debloated --tmpdir $TMPDIR/tabacco |& tee $TMPDIR/tabacco-output; then
    echo "fail. This command should not succeed and it did."
    failed_test=true
  else
    echo "Call to tabacco ran correctly, returning a nonzero exit code."
    if grep -q "When we varied uid and ran the program, we found a difference in execution behavior" $TMPDIR/tabacco-output
    then
      echo "Found expected string in output."
    else
      echo "Error, didnt see error message in $TMPDIR/tabacco-output, (did you adjust the error message and forget to adjust this test?)"
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
