#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

if [ $# -lt 3 ]; then
	echo "buildAndTest.sh debug|beta|release x64|x86|arm64|native|fat nosimd|simd"
	exit 1
fi

if [ "$3" == "simd" ]; then
	simd=1
elif [ "$3" == "nosimd" ]; then
	simd=0
else
	echo "please specify 'nosimd' or 'simd'"
	exit 1
fi

mkdir -p ./output
./tools/BuildCpp.sh $1 $2 ./output/IMPDTest -I ./ ./tools/IMPDTest.cpp ./src/IMPD.cpp

cd ./tests
echo Good tests...
echo
../output/IMPDTest <goodTests.impd | diff --strip-trailing-cr goodResults.txt -
echo
echo Bad tests...
echo
../output/IMPDTest <badTests.impd | diff --strip-trailing-cr badResults.txt -
echo Seems fine
cd ..

# C sources for libpng and zlib
C_SRCS=(./externals/libpng/png.c ./externals/libpng/pngerror.c ./externals/libpng/pngget.c \
		./externals/libpng/pngmem.c ./externals/libpng/pngpread.c ./externals/libpng/pngread.c \
		./externals/libpng/pngrio.c ./externals/libpng/pngrtran.c ./externals/libpng/pngrutil.c \
		./externals/libpng/pngset.c ./externals/libpng/pngtrans.c ./externals/libpng/pngwio.c \
		./externals/libpng/pngwrite.c ./externals/libpng/pngwtran.c ./externals/libpng/pngwutil.c \
		./externals/zlib/adler32.c ./externals/zlib/compress.c ./externals/zlib/crc32.c \
		./externals/zlib/deflate.c ./externals/zlib/infback.c ./externals/zlib/inffast.c \
		./externals/zlib/inflate.c ./externals/zlib/inftrees.c ./externals/zlib/trees.c \
		./externals/zlib/uncompr.c ./externals/zlib/zutil.c)

# -ffp-contract=off is necessary to avoid issues with floating point optimizations that can cause differences in results
./tools/BuildCpp.sh $1 $2 ./output/IVG2PNG \
		-ffp-contract=off -UTARGET_OS_MAC ./tools/IVG2PNG.cpp -DNUXPIXELS_SIMD=$simd \
		-I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib \
		./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp \
		"${C_SRCS[@]}"

./tools/BuildCpp.sh $1 $2 ./output/PolygonMaskTest \
		-DNUXPIXELS_SIMD=$simd -I ./ -I ./externals \
		./tools/PolygonMaskTest.cpp ./externals/NuX/NuXPixels.cpp

echo Testing...
cd tests
echo Invalid IVG tests...
tmp=$(mktemp)
bash ../tools/testInvalidIVG.sh ../output/InvalidIVGTest | tee "$tmp"
diff --strip-trailing-cr invalidIVGResults.txt "$tmp"
rm "$tmp"
bash ../tools/testIVG.sh ../output/IVG2PNG
if [ -n "${SKIP_SVG:-}" ]; then
		echo "Skipping SVG tests"
elif command -v node >/dev/null 2>&1; then
	bash ../tools/testSVG.sh
else
	echo "Warning: Node.js not found, skipping SVG tests" >&2
fi
cd ..
./output/PolygonMaskTest
exit 0
