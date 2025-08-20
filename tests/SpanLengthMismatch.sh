#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
mkdir -p output
./tools/BuildCpp.sh debug native ./output/SpanLengthMismatch -DNUXPIXELS_SIMD=0 -DNUXPIXELS_MAX_SPAN=9 -I ./ -I ./externals tests/SpanLengthMismatch.cpp externals/NuX/NuXPixels.cpp
./output/SpanLengthMismatch "$@"
