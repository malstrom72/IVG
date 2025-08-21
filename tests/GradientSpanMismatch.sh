#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
mkdir -p output
./tools/BuildCpp.sh beta native ./output/GradientSpanMismatch -DNUXPIXELS_SIMD=0 -DNUXPIXELS_MAX_SPAN=9 -I ./ -I ./externals tests/GradientSpanMismatch.cpp externals/NuX/NuXPixels.cpp
./output/GradientSpanMismatch "$@"
