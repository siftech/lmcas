#!/usr/bin/env bash
set -ex

cd "$(dirname "${BASH_SOURCE[0]}")"

# Build neck project
mkdir -p build
cd build
touch .nobackup
CC=clang CXX=clang++ cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja -j $(nproc)
