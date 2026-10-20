@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
REM Builds output\IVGFuzzReplaySanitized, the fuzz target's replay main() with AddressSanitizer, optimized but with
REM asserts on like tools\buildIVGFuzz.cmd, and replays every past crash input and the committed corpus through it. The
REM plain replays in build.cmd and replayFuzzCorpus.cmd only catch crashes and failed asserts; this also catches memory
REM errors without a long fuzz run. Unlike replayFuzzSanitized.sh it has no UndefinedBehaviorSanitizer, which MSVC
REM does not provide. The 8 MB stack is needed for AddressSanitizer's larger frames, as in buildIVGFuzz.cmd.
IF NOT EXIST output MKDIR output
SET CPP_OPTIONS=/fsanitize=address /U NDEBUG /D LIBFUZZ /D FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
CALL tools\BuildCpp.cmd release x64 output\IVGFuzzReplaySanitized tools\FuzzMain.cpp tools\IVG2PNG.cpp src\IVG.cpp src\IMPD.cpp externals\NuX\NuXPixels.cpp /I. /Iexternals /Iexternals\libpng /Iexternals\zlib /link /STACK:8388608 || EXIT /B 1
CALL tools\copyAsanRuntime.cmd || EXIT /B 1
FOR %%f IN (tests\fuzz\IVGFuzzCrashes\*) DO (
	output\IVGFuzzReplaySanitized.exe "%%f" >NUL || EXIT /B 1
)
IF EXIST output\fuzzCorpusSanitized RMDIR /S /Q output\fuzzCorpusSanitized
MKDIR output\fuzzCorpusSanitized
tar -xzf tests\fuzz\IVGFuzzCorpus.tar.gz -C output\fuzzCorpusSanitized || EXIT /B 1
DIR /B /S /A-D output\fuzzCorpusSanitized | output\IVGFuzzReplaySanitized.exe - || EXIT /B 1
EXIT /B 0
