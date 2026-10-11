#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Builds tools/IVGDigest.cpp with the scalar and with the SIMD rasterizer, renders every .ivg under tests, every past
# crash input and the committed corpus with both, and fails if any outcome, bounds or pixels differ. The regression
# images already hold both rasterizers to the same result for the tests; this extends that to the fuzz inputs. Both
# builds cap canvases as the fuzz builds do, so huge corpus inputs stay fast. CI runs it on Linux.
mkdir -p ./output
for simd in 0 1; do
	CPP_OPTIONS="-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION" bash ./tools/BuildCpp.sh release native \
			"./output/IVGDigest$simd" -DNUXPIXELS_SIMD=$simd -I ./ -I ./externals ./tools/IVGDigest.cpp ./src/IVG.cpp \
			./src/IMPD.cpp ./externals/NuX/NuXPixels.cpp
done
rm -rf ./output/fuzzCorpusSIMDDiff
mkdir -p ./output/fuzzCorpusSIMDDiff
tar -xzf ./tests/fuzz/IVGFuzzCorpus.tar.gz -C ./output/fuzzCorpusSIMDDiff
{
	find ./tests -type f -name '*.ivg'
	find ./tests/fuzz/IVGFuzzCrashes ./output/fuzzCorpusSIMDDiff -type f
} | sort > ./output/simdDiffInputs.txt
./output/IVGDigest0 - < ./output/simdDiffInputs.txt > ./output/simdDiff0.txt
./output/IVGDigest1 - < ./output/simdDiffInputs.txt > ./output/simdDiff1.txt
if ! cmp -s ./output/simdDiff0.txt ./output/simdDiff1.txt; then
	diff ./output/simdDiff0.txt ./output/simdDiff1.txt | head -20
	echo "The scalar and SIMD rasterizers render differently" >&2
	exit 1
fi
echo "The scalar and SIMD rasterizers render $(wc -l < ./output/simdDiffInputs.txt) inputs alike"
