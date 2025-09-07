@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests"

SET update=0
IF /I "%~1"=="update" (
    SET update=1
    SHIFT
)
IF "%~1"=="" (
    SET exe="..\output\InvalidIVGTest"
) ELSE (
    SET exe="%~1"
)

IF %update%==1 (
    (FOR %%f IN (ivg\invalid\*.ivg) DO (
        %exe% "%%f"
    )) >invalidIVGResults.txt
    TYPE invalidIVGResults.txt
    EXIT /b 0
) ELSE (
    SET fail=0
    FOR %%f IN (ivg\invalid\*.ivg) DO (
        %exe% "%%f"
        IF ERRORLEVEL 1 SET fail=1
    )
    EXIT /b %fail%
)
