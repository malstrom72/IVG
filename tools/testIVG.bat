@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

IF "%~1"=="" (
	SET exe="../output/IVG2PNG"
) ELSE (
	SET exe="%~1"
)

SET tempDir=%TEMP%\temp%RANDOM%
ECHO Using temporary dir: %tempDir%
MKDIR %tempDir%

FOR %%f IN (ivg\*.ivg) DO (
	ECHO Doing %%f
	ECHO.
	%exe% %%f %tempDir%\%%~nf.png || GOTO error
	fc %tempDir%\%%~nf.png png\%%~nf.png || GOTO error
	ECHO.
	ECHO.
)
ECHO.
ECHO ALL GOOD!!
ECHO.
DEL /q %tempDir%\*.png
RMDIR %tempDir%

GOTO :eof

:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
