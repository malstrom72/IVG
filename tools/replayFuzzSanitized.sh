#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Builds output/IVGFuzzReplaySanitized, the fuzz target's replay main() with AddressSanitizer and
# UndefinedBehaviorSanitizer, optimized but with asserts on like tools/buildIVGFuzz.sh, and replays every past crash
# input and the committed corpus through it. The plain replays in build.sh and replayFuzzCorpus.sh only catch crashes
# and failed asserts; this also catches memory errors and undefined behaviour without a long fuzz run. CI runs it on
# Linux. Set CPP_COMPILER to use another clang, e.g. Homebrew's on macOS.
mkdir -p ./output
OPTIONS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -UNDEBUG"
CPP_COMPILER="${CPP_COMPILER:-clang++}" CPP_OPTIONS="$OPTIONS -DLIBFUZZ -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION" \
		bash ./tools/BuildCpp.sh release native ./output/IVGFuzzReplaySanitized ./tools/FuzzMain.cpp ./tools/IVG2PNG.cpp \
		./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp -I ./ -I ./externals -I ./externals/libpng \
		-I ./externals/zlib
./output/IVGFuzzReplaySanitized ./tests/fuzz/IVGFuzzCrashes/*
rm -rf ./output/fuzzCorpusSanitized
mkdir -p ./output/fuzzCorpusSanitized
tar -xzf ./tests/fuzz/IVGFuzzCorpus.tar.gz -C ./output/fuzzCorpusSanitized
find ./output/fuzzCorpusSanitized -type f | ./output/IVGFuzzReplaySanitized -
