@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests"

IF "%~1"=="" (
		SET exe="..\output\InvalidIVGTest"
) ELSE (
		SET exe="%~1"
)

FOR %%f IN (ivg\invalid\*.ivg) DO (
		%exe% "%%f" || GOTO error
)
EXIT /b 0
:error
ECHO Error %ERRORLEVEL%
EXIT /b %ERRORLEVEL%
