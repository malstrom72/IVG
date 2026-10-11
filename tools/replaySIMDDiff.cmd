@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
REM Builds tools\IVGDigest.cpp with the scalar and with the SIMD rasterizer, renders every .ivg under tests, every past
REM crash input and the committed corpus with both, and fails if any outcome, bounds or pixels differ. The regression
REM images already hold both rasterizers to the same result for the tests; this extends that to the fuzz inputs. Both
REM builds cap canvases as the fuzz builds do, so huge corpus inputs stay fast. The 8 MB stack matches IVGFuzzReplay.
IF NOT EXIST output MKDIR output
SET CPP_OPTIONS=/D FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
FOR %%s IN (0 1) DO (
	CALL tools\BuildCpp.cmd release x64 output\IVGDigest%%s "-DNUXPIXELS_SIMD=%%s" /I. /Iexternals tools\IVGDigest.cpp src\IVG.cpp src\IMPD.cpp externals\NuX\NuXPixels.cpp /link /STACK:8388608 || EXIT /B 1
)
IF EXIST output\fuzzCorpusSIMDDiff RMDIR /S /Q output\fuzzCorpusSIMDDiff
MKDIR output\fuzzCorpusSIMDDiff
tar -xzf tests\fuzz\IVGFuzzCorpus.tar.gz -C output\fuzzCorpusSIMDDiff || EXIT /B 1
DIR /B /S /A-D tests\*.ivg tests\fuzz\IVGFuzzCrashes output\fuzzCorpusSIMDDiff > output\simdDiffInputs.txt || EXIT /B 1
output\IVGDigest0.exe - < output\simdDiffInputs.txt > output\simdDiff0.txt || EXIT /B 1
output\IVGDigest1.exe - < output\simdDiffInputs.txt > output\simdDiff1.txt || EXIT /B 1
FC /B output\simdDiff0.txt output\simdDiff1.txt >NUL
IF ERRORLEVEL 1 (
	FC output\simdDiff0.txt output\simdDiff1.txt
	ECHO The scalar and SIMD rasterizers render differently
	EXIT /B 1
)
ECHO The scalar and SIMD rasterizers render alike
EXIT /B 0
