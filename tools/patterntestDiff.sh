#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

OUT_DIR="output/patterntest-span"
SPANS="7 8 9 10"
mkdir -p "$OUT_DIR"

./tools/BuildCpp.sh release native ./output/PNGDiff -I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib ./tools/PNGDiff.cpp ./externals/libpng/*.c ./externals/zlib/*.c

for span in $SPANS; do
	./tools/BuildCpp.sh release native ./output/IVG2PNG -ffp-contract=off -DNUXPIXELS_MAX_SPAN=$span -DNUXPIXELS_SIMD=0 ./tools/IVG2PNG.cpp -I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib ./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp ./externals/libpng/*.c ./externals/zlib/*.c
	./output/IVG2PNG tests/ivg/patterntest.ivg "$OUT_DIR/patterntest_span${span}.png"
done

./output/PNGDiff "$OUT_DIR/patterntest_span7.png" "$OUT_DIR/patterntest_span8.png" "$OUT_DIR/patterntest_diff_7-8.png"
./output/PNGDiff "$OUT_DIR/patterntest_span8.png" "$OUT_DIR/patterntest_span9.png" "$OUT_DIR/patterntest_diff_8-9.png"
./output/PNGDiff "$OUT_DIR/patterntest_span9.png" "$OUT_DIR/patterntest_span10.png" "$OUT_DIR/patterntest_diff_9-10.png"
