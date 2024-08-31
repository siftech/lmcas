# You should source this script in your shell's rc file (e.g. bashrc or zshrc).
# Currently, this is only designed to support bash and zsh; users of other
# shells should ensure it works identically before blindly trusting it.

# Enables shellcheck (https://github.com/koalaman/shellcheck/), a helpful
# shell linter.
# shellcheck shell=bash

# bash and zsh report the name of a sourced script differently; this idiom
# should be compatible with either.
export LMCAS_CODE
LMCAS_CODE="$(realpath "$(dirname "$(realpath "${BASH_SOURCE[0]:-"$0"}")")/..")"

# Add our scripts to PATH.
export PATH="$LMCAS_CODE/tools/common:$PATH"
if [[ -z "${IN_LMCAS_CONTAINER:+x}" ]]; then
	export PATH="$LMCAS_CODE/tools/host:$PATH"
else
	export PATH="$LMCAS_CODE/tools/ctr:$PATH"
fi

# Add the lmcas cdr alias.
alias lcdr='cd "${LMCAS_CODE}/test/results/$(ls "${LMCAS_CODE}/test/results" | sort -r | head -n1)"'
