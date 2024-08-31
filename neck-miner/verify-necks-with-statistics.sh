#!/bin/bash

TEMPD=$(mktemp -d)
TIMESTAMP=$(date +%s)
OUT_PATH=${TEMPD}/${TIMESTAMP}-statistics.json

echo "Writing statistics to: ${OUT_PATH}"
python3 ./neck_verification/verify_necks.py ./neck_verification/verify_necks_config.json --stats $OUT_PATH