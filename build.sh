#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"

./tools/buildAndTest.sh beta native nosimd
./tools/buildAndTest.sh release native nosimd

sys="$(uname -s)"
if [ "$sys" = "Darwin" ] || [ "$sys" = "Linux" ]; then
    ./tools/buildAndTest.sh beta native simd
    ./tools/buildAndTest.sh release native simd
fi

if command -v emcc >/dev/null 2>&1; then
    bash ./tools/ivgfiddle/buildIVGFiddle.sh
else
    echo "Warning: skipping ivgfiddle build; requires Emscripten" >&2
fi

echo
echo ALL GOOD!!
echo
