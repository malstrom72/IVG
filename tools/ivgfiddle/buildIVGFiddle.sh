#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"
output=${1:-./output}
src=${2:-./src}
fonts=../../fonts
which -s emcc || brew install emscripten
mkdir -p "$output"
emcc -sWASM=1 -sNO_DISABLE_EXCEPTION_CATCHING -sTOTAL_MEMORY=67108864 -sSINGLE_FILE -sEXPORTED_FUNCTIONS=_rasterizeIVG,_deallocatePixels,_malloc,_free -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,lengthBytesUTF8,stringToUTF8,FS --embed-file "$fonts/sans-serif.ivgfont" --embed-file "$fonts/serif.ivgfont" --embed-file "$fonts/code.ivgfont" --embed-file "$src/demoSource.ivg@demoSource.ivg" -s "BINARYEN_METHOD='native-wasm'" -DNUXPIXELS_SIMD=0 -DNDEBUG -std=c++03 -fexceptions -O3 -I../../externals/ "$src/rasterizeIVG.cpp" ../../src/IVG.cpp ../../src/IMPD.cpp ../../externals/NuX/NuXPixels.cpp -o "$output/rasterizeIVG.js"
cp "$src/ivgfiddle.html" "$src/ivgfiddle.js" "$src/setupModule.js" "$output"
mkdir -p "$output/ace"
cp "$src/ace/ace.js" "$src/ace/ext-searchbox.js" "$src/ace/mode-ivg.js" "$src/ace/theme-twilight.js" "$output/ace"
