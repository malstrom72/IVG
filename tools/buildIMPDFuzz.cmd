@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
IF NOT EXIST output MKDIR output
REM Builds output\IMPDFuzz, the IMPD-only target in tools\IMPDFuzz.cpp, with the options, stack and AddressSanitizer
REM runtime of tools\buildIVGFuzz.cmd, without UndefinedBehaviorSanitizer, which MSVC does not provide. libFuzzer
REM takes folders only, so copy tests\*.impd into a seed folder and run it like IVGFuzz, e.g.
REM output\IMPDFuzz.exe -dict=tests/fuzz/IVGFuzz.dict -artifact_prefix=output\impdFuzzArtifacts\ output\impdFuzzCorpus
REM output\impdSeeds output\fuzzSeeds
SET CPP_OPTIONS=/fsanitize=address /fsanitize=fuzzer /U NDEBUG /D FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
CALL tools\BuildCpp.cmd release x64 output\IMPDFuzz tools\IMPDFuzz.cpp src\IMPD.cpp /I. /link /STACK:8388608 || EXIT /B 1
CALL tools\copyAsanRuntime.cmd || EXIT /B 1
