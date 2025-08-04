#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
CPP_OPTIONS="-fsanitize=fuzzer,address" bash ./tools/BuildCpp.sh beta native ./output/IVGFuzz -UTARGET_OS_MAC -DLIBFUZZ -I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib ./tools/IVG2PNG.cpp ./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp ./externals/libpng/*.c ./externals/zlib/*.c
