@ECHO OFF
SETLOCAL
CD /D "%~dp0\.."
IF NOT EXIST output MKDIR output
CALL tools\BuildCpp.cmd debug native output\SpanLengthMismatch -DNUXPIXELS_SIMD=0 -DNUXPIXELS_MAX_SPAN=9 -I . -I externals tests\SpanLengthMismatch.cpp externals\NuX\NuXPixels.cpp || GOTO error
output\SpanLengthMismatch %*
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
