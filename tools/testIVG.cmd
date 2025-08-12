@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests"

IF "%~1"=="" (
    SET exe="../output/IVG2PNG"
) ELSE (
    SET exe="%~1"
)
SET fonts=..\fonts

SET tempDir=%TEMP%\temp%RANDOM%
ECHO Using temporary dir: %tempDir%
MKDIR %tempDir%

FOR %%f IN (ivg\*.ivg) DO (
    ECHO Doing %%f
    ECHO.
    %exe% --fonts %fonts% "%%f" "%tempDir%\%%~nf.png" || GOTO error
    fc "%tempDir%\%%~nf.png" "png\%%~nf.png" || GOTO error
    ECHO.
    ECHO.
)
ECHO.
ECHO ALL GOOD!!
ECHO.
DEL /q %tempDir%\*.png
RMDIR %tempDir%

EXIT /b 0
:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
