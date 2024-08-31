#!/bin/bash

if [ -z "$1" ]; then
        echo "Must provide verify_neck json config path"
        exit
fi

if [ ! -f "$1" ]; then
        echo "verify_neck json config path is not a file"
        exit
fi

TIMESTAMP=$(date +%s)
TEMPD=$(mktemp -d /tmp/neck-miner-${TIMESTAMP}-XXXXXXXX)
OUT_PATH=${TEMPD}/${TIMESTAMP}-statistics.json
TABLE_PNG_PATH=${TEMPD}/${TIMESTAMP}-statistics.png
TABLE_MD_PATH=${TEMPD}/${TIMESTAMP}-statistics.md
TABLE_CSV_PATH=${TEMPD}/${TIMESTAMP}-statistics.csv

echo "Writing statistics to: ${OUT_PATH}"
python3 ./neck_verification/verify_necks.py $1 --stats $OUT_PATH --write-cfg $TEMPD
echo "Writing table to:"
echo "  ${TABLE_PNG_PATH}"
echo "  ${TABLE_MD_PATH}"
echo "  ${TABLE_CSV_PATH}"
python3 ./neck_verification/generate-statistics-plots.py $OUT_PATH --png $TABLE_PNG_PATH --markdown $TABLE_MD_PATH --csv $TABLE_CSV_PATH

for i in ${TEMPD}/*.dot; do
    dot -Tpng $i -o ${i%%.*}.png
done
