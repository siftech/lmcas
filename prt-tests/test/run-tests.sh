#!/bin/bash
set -o pipefail


PROGNAME=$(basename "$0")
warn()  { echo "$PROGNAME: ${@}" 1>&2; }
die()   { warn "${@}"; exit 1; }
dbug()   { test -z "$DEBUG" || warn "${@}"; }


thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd -P "$( dirname "$SOURCE" )" && /bin/pwd )"
dbug thisdir is "$thisdir"

maxtime='30m'

export TESTING=1

# Build the delivery image
./tools/host/lmcas-dev-shell bash -c 'nix build -L /code#deliv'

# Copy the delivery image to the shared FS
./tools/host/lmcas-dev-shell bash -c 'cp $(realpath result) deliv.tgz'

# Load the delivery image into the local daemon as lmcas-deliv:nightly
docker load < deliv.tgz

# Delete the now-loaded delivery image
rm deliv.tgz

cur_dir="$(pwd)"
cd "$thisdir" || exit
ALT_WGET_SPEC=$(realpath wget.json)
export ALT_WGET_SPEC
"$PRT_PATH"/timestamp echo Starting prt
timeout -k 5s $maxtime "$PRT_PATH"/prt "$@" 2>&1 |tee prt.log
retval=$?
"$PRT_PATH"/timestamp echo Finished prt

if [ $retval -eq 124 ]; then
  echo -e "\n\nERROR prt timed out after $maxtime"
elif [ $retval -ne 0 ]; then
  echo -e "\n\nERROR prt failed with exit code $retval"
else
  echo -e "\n\nNOTE prt succeeded, now going to clean up stuff...ignore kill-stuff errors after this"
fi

cd "$cur_dir" || exit

# would like to do these at end, regardless of prt result, but must stash the returnval of above to 
# determine if test failed and return that as overall exit val

docker rm -f lmcas

exit $retval
