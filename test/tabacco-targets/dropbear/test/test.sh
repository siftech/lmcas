#!/usr/bin/env bash

set -euo pipefail

bloated_flag=$1
log=$2

cd "/test/tabacco-targets/dropbear"

pytest "--$bloated_flag" "test/test_dropbear.py" -m "not broken" \
	--json-report --json-report-summary --json-report-file \
	"$log"
