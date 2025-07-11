@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

IF "%~1"=="" (
	SET exe=..\output\IVG2PNG.exe
) ELSE (
	SET exe=%~1
)

FOR %%f IN (ivg\*.ivg) DO (
	ECHO Doing %%f
	ECHO.
	%exe% %%f png\%%~nf.png || GOTO BAD
	ECHO.
	ECHO.
)
GOTO END

:BAD
ECHO OOPS!!

:END
ENDLOCAL
