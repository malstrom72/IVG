#!/bin/bash
set -e -o pipefail -u
cd "$(dirname "$0")"

./tools/buildAndTest.sh beta native nosimd
./tools/buildAndTest.sh release native nosimd

if [ "$(uname -s)" = "Darwin" ]; then
./tools/buildAndTest.sh beta native simd
./tools/buildAndTest.sh release native simd
fi
