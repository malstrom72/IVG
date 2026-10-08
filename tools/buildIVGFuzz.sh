#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
mkdir -p ./output
# Builds the libFuzzer target in IVG2PNG.cpp with clang, optimized but with asserts on, like buildIVGFuzz.cmd.
# tools/runIVGFuzz.sh runs it. On macOS, point CPP_COMPILER at Homebrew's llvm clang++ (see docs/fuzzing.md).
# UndefinedBehaviorSanitizer is on too, stopping at the first report so that libFuzzer saves the input. MSVC has no
# equivalent, so buildIVGFuzz.cmd builds without it.
OPTIONS="-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -UNDEBUG -DLIBFUZZ"
CPP_COMPILER="${CPP_COMPILER:-clang++}" CPP_OPTIONS="$OPTIONS -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION" \
		bash ./tools/BuildCpp.sh release native output/IVGFuzz tools/IVG2PNG.cpp src/IVG.cpp src/IMPD.cpp \
		externals/NuX/NuXPixels.cpp -I . -I externals -I externals/libpng -I externals/zlib
