#!/usr/bin/env bash

set -euo pipefail

bloated_flag=$1
log=$2

cd "/test/tabacco-targets/curl/test"

pytest "--$bloated_flag" -m "not broken" \
	--json-report --json-report-summary --json-report-file \
	"$log"
