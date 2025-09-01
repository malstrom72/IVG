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
%exe% "%%f" || GOTO error
)) >invalidIVGResults.txt
TYPE invalidIVGResults.txt
) ELSE (
FOR %%f IN (ivg\invalid\*.ivg) DO (
%exe% "%%f" || GOTO error
)
)

EXIT /b 0
:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
