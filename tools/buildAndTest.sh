#!/bin/bash
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

# -ffp-contract=off is necessary to avoid issues with floating point optimizations that can cause differences in results
./tools/BuildCpp.sh $1 $2 ./output/IVG2PNG -ffp-contract=off ./tools/IVG2PNG.cpp -DNUXPIXELS_SIMD=$simd -I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib ./src/IVG.cpp ./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp ./externals/libpng/*.c ./externals/zlib/*.c

echo Testing...
cd tests
../tools/testIVG.sh ../output/IVG2PNG
cd ..
exit 0
