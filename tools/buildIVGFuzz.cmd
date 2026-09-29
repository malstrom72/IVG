@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
IF NOT EXIST output MKDIR output
REM Builds the libFuzzer target in IVG2PNG.cpp with MSVC, optimized but with asserts on (release with NDEBUG undefined).
REM Copies the AddressSanitizer runtime next to it. tools\runIVGFuzz.cmd runs it.
REM The 8 MB stack matches Linux and macOS. With Windows' default 1 MB, AddressSanitizer's larger stack frames overflow
REM before IMPD's recursion limit is reached, which a release build handles within 1 MB.
SET CPP_OPTIONS=/fsanitize=address /fsanitize=fuzzer /U NDEBUG /D LIBFUZZ /D FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
CALL tools\BuildCpp.cmd release x64 output\IVGFuzz tools\IVG2PNG.cpp src\IVG.cpp src\IMPD.cpp externals\NuX\NuXPixels.cpp /I. /Iexternals /Iexternals\libpng /Iexternals\zlib /link /STACK:8388608 || EXIT /B 1
CALL tools\copyAsanRuntime.cmd || EXIT /B 1
