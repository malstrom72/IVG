#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"
output=${1:-./output}
src=${2:-./src}
fonts=../../fonts
if ! command -v emcc >/dev/null 2>&1; then
 echo "Warning: Emscripten not found, skipping ivgfiddle build" >&2
 exit 0
fi
mkdir -p "$output"
emcc -sWASM=1 -sNO_DISABLE_EXCEPTION_CATCHING -sTOTAL_MEMORY=67108864 -sSINGLE_FILE -sMODULARIZE=1 -sEXPORT_ES6=0 -sENVIRONMENT=web,node -sEXPORTED_FUNCTIONS=_rasterizeIVG,_deallocatePixels,_malloc,_free -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,lengthBytesUTF8,stringToUTF8,FS,HEAPU8,HEAPU32,HEAPF32 -include "$src/uint32_char_traits.h" --embed-file "$fonts/sans-serif.ivgfont@sans-serif.ivgfont" --embed-file "$fonts/serif.ivgfont@serif.ivgfont" --embed-file "$fonts/code.ivgfont@code.ivgfont" --embed-file "$src/demoSource.ivg@demoSource.ivg" -s "BINARYEN_METHOD='native-wasm'" -DNUXPIXELS_SIMD=0 -DNDEBUG -std=c++03 -fexceptions -O3 -I../../externals/ "$src/rasterizeIVG.cpp" ../../src/IVG.cpp ../../src/IMPD.cpp ../../externals/NuX/NuXPixels.cpp -o "$output/rasterizeIVG.js"
cp "$src/ivgfiddle.html" "$src/ivgfiddle.js" "$src/setupModule.js" "$output"
mkdir -p "$output/ace"
cp "$src/ace/ace.js" "$src/ace/ext-searchbox.js" "$src/ace/mode-ivg.js" "$src/ace/theme-twilight.js" "$output/ace"
if command -v node >/dev/null 2>&1; then
 node ./testIVGFiddle.js "$output"
else
 echo "Warning: Node.js not found, skipping ivgfiddle test" >&2
fi
