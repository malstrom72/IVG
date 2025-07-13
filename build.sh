#!/bin/bash
set -e -o pipefail -u
cd "$(dirname "$0")"

./tools/buildAndTest.sh beta native nosimd
./tools/buildAndTest.sh release native nosimd

sys="$(uname -s)"
if [ "$sys" = "Darwin" ] || [ "$sys" = "Linux" ]; then
    ./tools/buildAndTest.sh beta native simd
    ./tools/buildAndTest.sh release native simd
fi
