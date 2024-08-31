#!/usr/bin/env bash

set -euo pipefail

bloated_flag=$1
log=$2

cd "/test/tabacco-targets"

pytest "--$bloated_flag" "mongoose/test/test_mongoose.py" -m "not broken" \
	--json-report --json-report-summary --json-report-file \
	"$log"
