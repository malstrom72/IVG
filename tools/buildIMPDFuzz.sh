#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
mkdir -p ./output
# Builds output/IMPDFuzz, the IMPD-only target in tools/IMPDFuzz.cpp, with the sanitizers and options of
# tools/buildIVGFuzz.sh; on macOS, point CPP_COMPILER at Homebrew's llvm clang++. libFuzzer takes folders only, so copy
# tests/*.impd into a seed folder and run it like IVGFuzz, e.g. ./output/IMPDFuzz -dict=tests/fuzz/IVGFuzz.dict
# -artifact_prefix=output/impdFuzzArtifacts/ output/impdFuzzCorpus output/impdSeeds output/fuzzSeeds
OPTIONS="-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -UNDEBUG"
CPP_COMPILER="${CPP_COMPILER:-clang++}" CPP_OPTIONS="$OPTIONS -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION" \
		bash ./tools/BuildCpp.sh release native output/IMPDFuzz tools/IMPDFuzz.cpp src/IMPD.cpp -I .
