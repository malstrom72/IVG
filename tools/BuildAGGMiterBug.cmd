@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\.."

SET target=%1
IF "%target%"=="" SET target=release
SET model=%2
IF "%model%"=="" SET model=native

MKDIR .\output 2>NUL
CALL .\tools\BuildCpp.cmd %target% %model% .\output\AGGMiterBug ^
	/I"." ^
	/I"externals" ^
	/I"externals\libpng" ^
	/I"externals\zlib" ^
	/I"..\agg-2.6\agg-src\include" ^
	.\tools\AGGMiterBug.cpp ^
	.\externals\libpng\*.c ^
	.\externals\zlib\*.c ^
	..\agg-2.6\agg-src\src\*.cpp || GOTO error
EXIT /b 0
:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
