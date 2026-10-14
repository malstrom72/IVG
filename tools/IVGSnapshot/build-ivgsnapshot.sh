#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../..

C_SRCS=(
./externals/libpng/png.c
./externals/libpng/pngerror.c
./externals/libpng/pngget.c
./externals/libpng/pngmem.c
./externals/libpng/pngpread.c
./externals/libpng/pngread.c
./externals/libpng/pngrio.c
./externals/libpng/pngrtran.c
./externals/libpng/pngrutil.c
./externals/libpng/pngset.c
./externals/libpng/pngtrans.c
./externals/libpng/pngwio.c
./externals/libpng/pngwrite.c
./externals/libpng/pngwtran.c
./externals/libpng/pngwutil.c
./externals/zlib/adler32.c
./externals/zlib/compress.c
./externals/zlib/crc32.c
./externals/zlib/deflate.c
./externals/zlib/infback.c
./externals/zlib/inffast.c
./externals/zlib/inflate.c
./externals/zlib/inftrees.c
./externals/zlib/trees.c
./externals/zlib/uncompr.c
./externals/zlib/zutil.c
)

mkdir -p ./output
./tools/BuildCpp.sh beta native ./output/IVGSnapshot \
-ffp-contract=off -DNUXPIXELS_SIMD=0 \
-I ./ -I ./externals -I ./externals/libpng -I ./externals/zlib \
./tools/IVGSnapshot/IVGSnapshot.cpp ./src/IVG.cpp ./src/IMPD.cpp \
./externals/NuX/NuXThreads.cpp ./externals/NuX/NuXThreadsPosix.cpp \
./externals/NuX/NuXFiles.cpp ./externals/NuX/NuXFilesPosix.cpp \
./externals/NuX/NuXPixels.cpp \
"${C_SRCS[@]}"
