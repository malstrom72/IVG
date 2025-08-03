#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

mkdir -p ./output
CPP_COMPILER="${CPP_COMPILER:-clang++}"
CPP_OPTIONS="${CPP_OPTIONS:--fsanitize=fuzzer,address,undefined}"
if [ "$(uname -s)" = "Darwin" ]; then
CPP_OPTIONS+=" -UTARGET_OS_MAC"
fi
export CPP_COMPILER CPP_OPTIONS

./tools/BuildCpp.sh release native ./output/IVGFuzz -DLIBFUZZ -I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib ./tools/IVG2PNG.cpp ./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp ./externals/libpng/*.c ./externals/zlib/*.c
