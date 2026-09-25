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

# Emscripten compiles the library as C++03 with clang and libc++, and Apple's toolchain is the same pair, so on
# macOS the library is compile-checked that way. gcc cannot stand in: libstdc++ refuses <cstdint> below C++11.
if [ "$sys" = "Darwin" ]; then
	for src in src/IVG.cpp src/IMPD.cpp externals/NuX/NuXPixels.cpp; do
		CPP_OPTIONS="-std=c++03" ./tools/BuildCpp.sh release native "output/$(basename "$src" .cpp).cpp03.o" \
				"$src" -c -DNUXPIXELS_SIMD=0 -I ./ -I ./externals
	done
fi

if command -v emcc >/dev/null 2>&1; then
    bash ./tools/ivgfiddle/buildIVGFiddle.sh
else
    echo "Warning: skipping ivgfiddle build; requires Emscripten" >&2
fi

echo
echo "=== ALL BUILDS AND TESTS COMPLETED SUCCESSFULLY ==="
echo
