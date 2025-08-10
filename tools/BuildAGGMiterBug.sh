#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

target="${1:-release}"
model="${2:-native}"

mkdir -p ./output
./tools/BuildCpp.sh "$target" "$model" ./output/AGGMiterBug \
		-I ./ \
		-I ./externals \
		-I ./externals/libpng \
		-I ./externals/zlib \
		-I ../agg-2.6/agg-src/include \
		./tools/AGGMiterBug.cpp \
		./externals/libpng/*.c \
		./externals/zlib/*.c \
		../agg-2.6/agg-src/src/*.cpp
