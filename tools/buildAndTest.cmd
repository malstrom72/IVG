@ECHO OFF
CD /D "%~dp0\.."

SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

IF NOT "%~1"=="" IF NOT "%~2"=="" IF NOT "%~3"=="" GOTO argsOK
	ECHO buildAndTest debug^|beta^|release x86^|x64^|arm64 nosimd^|simd
	EXIT /B 1
:argsOK

IF "%~3" == "simd" (
	SET simd=1
) ELSE IF "%~3" == "nosimd" (
	SET simd=0
) ELSE (
	ECHO please specify 'nosimd' or 'simd'
	EXIT /B 1
)

MKDIR .\output 2>NUL
CALL .\tools\BuildCpp.cmd %1 %2 .\output\IMPDTest /I"." .\tools\IMPDTest.cpp .\src\IMPD.cpp || EXIT /B 1

CD tests
ECHO Good tests...
ECHO.
..\output\IMPDTest <goodtests.impd >goodcheck.txt || GOTO error
fc goodcheck.txt goodresults.txt || GOTO error
DEL goodcheck.txt
ECHO.
ECHO ### BAD TESTS ###

ECHO.
..\output\IMPDTest <badtests.impd >badcheck.txt || GOTO error
fc badcheck.txt badresults.txt || GOTO error
DEL badcheck.txt

ECHO Seems fine
CD ..

CALL .\tools\BuildCpp.cmd %1 %2 .\output\IVG2PNG "-DNUXPIXELS_SIMD=%simd%" /I"." /I"externals" /I"externals\libpng" /I"externals\zlib" .\tools\IVG2PNG.cpp .\src\IVG.cpp .\src\IMPD.cpp .\externals\NuX\NuXPixels.cpp .\externals\libpng\*.c .\externals\zlib\*.c || EXIT /B 1

ECHO Testing...
CD tests
CALL ..\tools\testIVG.cmd ..\output\IVG2PNG || GOTO error
CALL ..\tools\testSVG.cmd || GOTO error

GOTO :eof

:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
