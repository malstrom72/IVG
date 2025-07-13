@ECHO OFF
CD /D "%~dp0\..\tests"

..\output\IMPDTest <goodTests.impd >goodResults.txt || GOTO error
..\output\IMPDTest <badTests.impd >badResults.txt || GOTO error

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
