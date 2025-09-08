@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests"

SET update=0
IF /I "%~1"=="update" (
	SET update=1
	SHIFT
)
IF "%~1"=="" (
	SET exe="../output/IVG2PNG"
) ELSE (
	SET exe="%~1"
)
SET fonts=..\fonts
SET images=.

SET tempDir=%TEMP%\temp%RANDOM%
ECHO Using temporary dir: %tempDir%
MKDIR %tempDir%

SET fail=0
FOR %%f IN (ivg\*.ivg) DO (
	ECHO Doing %%f
	ECHO.
	IF "%%~nf"=="huge" (
		%exe% --fast --images %images% --fonts %fonts% "%%f" "%tempDir%\%%~nf.png"
	) ELSE (
		%exe% --images %images% --fonts %fonts% "%%f" "%tempDir%\%%~nf.png"
	)
	IF ERRORLEVEL 1 SET fail=1
	IF %update%==1 (
		COPY "%tempDir%\%%~nf.png" "png\%%~nf.png" >NUL
	) ELSE (
		fc "%tempDir%\%%~nf.png" "png\%%~nf.png"
		IF ERRORLEVEL 1 SET fail=1
	)
	ECHO.
	ECHO.
)
DEL /q %tempDir%\*.png >NUL 2>NUL
RMDIR %tempDir% >NUL 2>NUL
IF %fail% NEQ 0 (
	ECHO.
	ECHO === IVG TESTS FAILED === 1>&2
	ECHO.
)

EXIT /b %fail%
