@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests"

IF "%~1"=="" (
    SET exe=..\output\IVG2PNG.exe
) ELSE (
    SET exe=%~1
)
SET fonts=..\fonts

FOR %%f IN (ivg\*.ivg) DO (
    ECHO Doing %%f
    ECHO.
    %exe% --fonts %fonts% "%%f" "png\%%~nf.png" || GOTO BAD
    ECHO.
    ECHO.
)
GOTO END

:BAD
ECHO OOPS!!

:END
ENDLOCAL
EXIT /b 0
