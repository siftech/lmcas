#!/usr/bin/env bash

set -euo pipefail


log=$1

cd "/test/tabacco-targets/uwisc"

pytest --bloated --debloated "test/test_uwisc.py" -m "not broken" \
	--json-report --json-report-summary --json-report-file \
	"$log"
