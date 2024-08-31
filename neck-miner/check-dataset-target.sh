#!/usr/bin/env bash

set -e

DIRNAME=$(dirname "${BASH_SOURCE[0]}")

NECK_TARGET=$1
DATA_SET=$2
OUT_DIR=$3

# NOTE: OUT_DIR is always a subdirectory of ./debug

if [ -z "$NECK_TARGET" ] ; then
  echo "Must provide neck target name"
  echo "Usage:"
  echo "$0: neck-target [data-set [out-dir]]"
  echo "  neck-target is a program under test ll file."
  echo "  data-set may be one of: slash_dataset or tabacco_dataset"
  echo "    The default is slash_dataset."
  echo "  out-dir is where you want the output."
  echo "    The default is: ./debug/<data-set>/<neck-miner>"
  exit 1
fi

# valid data sets are: slash-dataset tabacco-dataset
if [ ! -d "$DIRNAME/external/$DATA_SET" ] ; then
  echo "Using datasource of slash-dataset."
  DATA_SET="slash-dataset"
fi

if [ -z "$OUT_DIR" ] ; then
  OUT_DIR="$DIRNAME/debug/$DATA_SET/$NECK_TARGET"
  mkdir -p "$OUT_DIR" && touch "$DIRNAME/debug/.nobackup"
fi

if [ ! -d "$OUT_DIR" ] ; then
  echo "Must provide an output directory"
  exit 1
fi

DATASET_TARGET="$DIRNAME/external/$DATA_SET/$NECK_TARGET.ll"

if [ ! -e "$DATASET_TARGET" ] ; then
  echo "$DATASET_TARGET does not exist"
fi

P_FUNC_DIR="$OUT_DIR/pfunc"
mkdir -p "$P_FUNC_DIR"

time $DIRNAME/build/tools/neck-miner/neck-miner \
	--debug \
	-m $DATASET_TARGET \
	-c $DIRNAME/config/cmd-tool-config.json \
	--function-local-points-to-info-wo-globals \
	--use-simplified-dfa \
	--write-cfg $OUT_DIR/$NECK_TARGET \
	--write-participating-functions-cfg $P_FUNC_DIR/$NECK_TARGET \
	--verify-neck \
	--neck-placement $OUT_DIR/$NECK_TARGET.json \
	2> $OUT_DIR/$NECK_TARGET.err \
	| tee $OUT_DIR/$NECK_TARGET.out
