@ECHO OFF
CD /D "%~dp0"

SET output=%1
IF "%output%"=="" SET output=.\output
SET src=%2
IF "%src%"=="" SET src=.\src
SET fonts=..\..\fonts

where emcc >NUL 2>&1
IF ERRORLEVEL 1 (
    ECHO emcc not found. Install Emscripten first.
    GOTO error
)

IF NOT EXIST "%output%" MKDIR "%output%"
"emcc" -sWASM=1 -sNO_DISABLE_EXCEPTION_CATCHING -sTOTAL_MEMORY=67108864 -sSINGLE_FILE -sMODULARIZE=1 -sEXPORT_ES6=0 -sENVIRONMENT=web,node -sEXPORTED_FUNCTIONS=_rasterizeIVG,_deallocatePixels,_malloc,_free -sEXPORTED_RUNTIME_METHODS=ccall,cwrap,lengthBytesUTF8,stringToUTF8,FS,HEAPU8,HEAPU32,HEAPF32 -include "%src%\uint32_char_traits.h" --embed-file "%fonts%\sans-serif.ivgfont@sans-serif.ivgfont" --embed-file "%fonts%\serif.ivgfont@serif.ivgfont" --embed-file "%fonts%\code.ivgfont@code.ivgfont" --embed-file "%src%\demoSource.ivg@demoSource.ivg" -s "BINARYEN_METHOD='native-wasm'" -DNUXPIXELS_SIMD=0 -DNDEBUG -std=c++03 -fexceptions -O3 -I../../externals/ "%src%\rasterizeIVG.cpp" ../../src/IVG.cpp ../../src/IMPD.cpp ../../externals/NuX/NuXPixels.cpp -o "%output%\rasterizeIVG.js" || GOTO error

COPY "%src%\ivgfiddle.html" "%src%\ivgfiddle.js" "%src%\setupModule.js" "%output%" >NUL
IF NOT EXIST "%output%\ace" MKDIR "%output%\ace"
COPY "%src%\ace\ace.js" "%src%\ace\ext-searchbox.js" "%src%\ace\mode-ivg.js" "%src%\ace\theme-twilight.js" "%output%\ace" >NUL

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
