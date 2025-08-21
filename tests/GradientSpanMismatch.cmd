@ECHO OFF
SETLOCAL
CD /D "%~dp0\.."
MKDIR output 2>NUL
CALL tools\BuildCpp.cmd debug native output\GradientSpanMismatch -DNUXPIXELS_SIMD=0 -DNUXPIXELS_MAX_SPAN=9 -I . -I externals tests\GradientSpanMismatch.cpp externals\NuX\NuXPixels.cpp || GOTO error
output\GradientSpanMismatch %*
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
