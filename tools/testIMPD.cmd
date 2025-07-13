@ECHO OFF
CD /D "%~dp0\..\tests"

ECHO.
ECHO ### GOOD TESTS ###
ECHO.
.\Debug\ImpDTest <goodTests.impd >goodCheck.txt || GOTO error
FC goodcheck.txt goodresults.txt || GOTO error
DEL goodcheck.txt
ECHO.
ECHO ### BAD TESTS ###
ECHO.
.\Debug\ImpDTest <badTests.impd >badcheck.txt || GOTO error
FC badcheck.txt badresults.txt || GOTO error
DEL badcheck.txt

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
