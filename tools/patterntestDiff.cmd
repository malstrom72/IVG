@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\.."

SET OUT_DIR=output\patterntest-span
SET IVG=tests\ivg\patterntest.ivg
IF NOT EXIST "%OUT_DIR%" MKDIR "%OUT_DIR%"

CALL tools\BuildCpp.cmd release native output\PNGDiff /I"." /I"externals" /I"externals\\libpng" /I"externals\\zlib" tools\PNGDiff.cpp externals\libpng\*.c externals\zlib\*.c || GOTO error

FOR %%S IN (7 8 9 10) DO (
	CALL tools\\BuildCpp.cmd release native output\\IVG2PNG -ffp-contract=off "-DNUXPIXELS_MAX_SPAN=%%S" "-DNUXPIXELS_SIMD=0" /I"." /I"externals" /I"externals\\libpng" /I"externals\\zlib" tools\\IVG2PNG.cpp src\\IVG.cpp src\\IMPD.cpp externals\\NuX\\NuXPixels.cpp externals\\libpng\\*.c externals\\zlib\\*.c || GOTO error
	output\\IVG2PNG %IVG% %OUT_DIR%\\patterntest_span%%S.png || GOTO error
)

output\\PNGDiff %OUT_DIR%\\patterntest_span7.png %OUT_DIR%\\patterntest_span8.png %OUT_DIR%\\patterntest_diff_7-8.png || GOTO error
output\\PNGDiff %OUT_DIR%\\patterntest_span8.png %OUT_DIR%\\patterntest_span9.png %OUT_DIR%\\patterntest_diff_8-9.png || GOTO error
output\\PNGDiff %OUT_DIR%\\patterntest_span9.png %OUT_DIR%\\patterntest_span10.png %OUT_DIR%\\patterntest_diff_9-10.png || GOTO error

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
