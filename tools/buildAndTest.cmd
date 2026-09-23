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

REM C sources for libpng and zlib
SET CSOURCES=
FOR %%f IN (
	png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c
	pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c
	pngwrite.c pngwtran.c pngwutil.c
) DO (
	SET "CSOURCES=!CSOURCES! externals\libpng\%%f"
)
FOR %%f IN (
	adler32.c compress.c crc32.c deflate.c infback.c inffast.c
	inflate.c inftrees.c trees.c uncompr.c zutil.c
) DO (
	SET "CSOURCES=!CSOURCES! externals\zlib\%%f"
)

CALL .\tools\BuildCpp.cmd %1 %2 .\output\IVG2PNG "-DNUXPIXELS_SIMD=%simd%" ^
		/I"." /I"externals" /I"externals\libpng" /I"externals\zlib" ^
		.\tools\IVG2PNG.cpp .\src\IVG.cpp .\src\IMPD.cpp .\externals\NuX\NuXPixels.cpp ^
		.\externals\NuX\NuXFiles.cpp .\externals\NuX\NuXFilesWin32.cpp ^
		%CSOURCES% || EXIT /B 1

CALL .\tools\BuildCpp.cmd %1 %2 .\output\InvalidIVGTest ^
		"-DNUXPIXELS_SIMD=%simd%" /I"." /I"externals" ^
		.\tests\invalidIVG.cpp .\src\IVG.cpp .\src\IMPD.cpp .\externals\NuX\NuXPixels.cpp || EXIT /B 1

CALL .\tools\BuildCpp.cmd %1 %2 .\output\PolygonMaskTest ^
		"-DNUXPIXELS_SIMD=%simd%" /I"." /I"externals" ^
		.\tools\PolygonMaskTest.cpp .\externals\NuX\NuXPixels.cpp || EXIT /B 1

CALL .\tools\BuildCpp.cmd %1 %2 .\output\IVGSnapshot "-DNUXPIXELS_SIMD=%simd%" ^
		/I"." /I"externals" /I"externals\libpng" /I"externals\zlib" ^
		.\tools\IVGSnapshot\IVGSnapshot.cpp .\src\IVG.cpp .\src\IMPD.cpp ^
		.\externals\NuX\NuXThreads.cpp .\externals\NuX\NuXThreadsWin32.cpp ^
		.\externals\NuX\NuXFiles.cpp .\externals\NuX\NuXFilesWin32.cpp ^
		.\externals\NuX\NuXPixels.cpp %CSOURCES% || EXIT /B 1

CALL .\tools\BuildCpp.cmd %1 %2 .\output\TestSnapshotPlan ^
		"-DIVG_SNAPSHOT_TESTING=1" "-DNUXPIXELS_SIMD=%simd%" ^
		/I"." /I"externals" /I"externals\libpng" /I"externals\zlib" ^
		.\tools\IVGSnapshot\tests\TestSnapshotPlan.cpp .\src\IVG.cpp .\src\IMPD.cpp ^
		.\externals\NuX\NuXThreads.cpp .\externals\NuX\NuXThreadsWin32.cpp ^
		.\externals\NuX\NuXFiles.cpp .\externals\NuX\NuXFilesWin32.cpp ^
		.\externals\NuX\NuXPixels.cpp %CSOURCES% || EXIT /B 1

ECHO Testing...
CD tests
ECHO Invalid IVG tests...
CALL ..\tools\testInvalidIVG.cmd ..\output\InvalidIVGTest >invalidIVGCheck.txt
SET err=%ERRORLEVEL%
TYPE invalidIVGCheck.txt
IF NOT "%err%"=="0" GOTO error
REM Sorted on both sides before comparing: each harness walks the fixtures in its own
REM platform's order, and which order that is says nothing about the results.
SORT invalidIVGCheck.txt >"%TEMP%\invalidIVGCheck.sorted"
SORT invalidIVGResults.txt >"%TEMP%\invalidIVGResults.sorted"
fc "%TEMP%\invalidIVGCheck.sorted" "%TEMP%\invalidIVGResults.sorted" || GOTO error
DEL invalidIVGCheck.txt "%TEMP%\invalidIVGCheck.sorted" "%TEMP%\invalidIVGResults.sorted"
CALL ..\tools\testIVG.cmd ..\output\IVG2PNG || GOTO error
IF NOT "%SKIP_SVG%"=="" (
	ECHO Skipping SVG tests
) ELSE (
	WHERE node >NUL 2>NUL
	IF ERRORLEVEL 1 (
		ECHO Warning: Node.js not found, skipping SVG tests
	) ELSE (
		CALL ..\tools\testSVG.cmd || GOTO error
	)
)
CD ..
CALL .\output\PolygonMaskTest || GOTO error
CALL .\output\TestSnapshotPlan || GOTO error
CALL :listOnly ListOnlySample || GOTO error
CALL :listOnly ListScenarioVariants || GOTO error
GOTO :eof

REM Compares one --list-only run against its golden. IVGSnapshot prints paths through NuXFiles::Path,
REM which uses the platform separator, so the separators are normalised here rather than keeping a
REM second golden. findstr numbers the lines so that FOR does not swallow the blank ones, and the
REM line is captured with delayed expansion off so that a "!" in it survives.
:listOnly
SET "check=%TEMP%\%~1.check"
SET "norm=%TEMP%\%~1.norm"
.\output\IVGSnapshot --list-only tools/IVGSnapshot/tests/%~1.ivg >"%check%" || EXIT /B 1
(FOR /F "usebackq delims=" %%L IN (`findstr /n "^" "%check%"`) DO (
	SETLOCAL DISABLEDELAYEDEXPANSION
	SET "line=%%L"
	SETLOCAL ENABLEDELAYEDEXPANSION
	SET "line=!line:*:=!"
	IF DEFINED line (ECHO(!line:\=/!) ELSE (ECHO()
	ENDLOCAL
	ENDLOCAL
)) >"%norm%"
FC "%norm%" ".\tools\IVGSnapshot\tests\%~1.txt" || EXIT /B 1
DEL "%check%" "%norm%"
EXIT /B 0

:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
