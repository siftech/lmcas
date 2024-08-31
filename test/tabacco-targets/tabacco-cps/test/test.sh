#!/usr/bin/env bash

set -euo pipefail


log=$1

cd "/test/tabacco-targets/tabacco-cps"

pytest --bloated --debloated "test/test_tabacco_cps.py" -m "not broken" \
	--json-report --json-report-summary --json-report-file \
	"$log"
