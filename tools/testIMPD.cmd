@ECHO OFF
CD /D "%~dp0\..\tests"

SET update=0
IF /I "%~1"=="update" (
SET update=1
SHIFT
)

IF %update%==1 (
ECHO.
ECHO ### GOOD TESTS ###
ECHO.
.\Debug\ImpDTest <goodTests.impd >goodResults.txt || GOTO error
TYPE goodResults.txt
ECHO.
ECHO ### BAD TESTS ###
ECHO.
.\Debug\ImpDTest <badTests.impd >badResults.txt || GOTO error
TYPE badResults.txt
) ELSE (
ECHO.
ECHO ### GOOD TESTS ###
ECHO.
.\Debug\ImpDTest <goodTests.impd >goodcheck.txt || GOTO error
FC goodcheck.txt goodresults.txt || GOTO error
DEL goodcheck.txt
ECHO.
ECHO ### BAD TESTS ###
ECHO.
.\Debug\ImpDTest <badTests.impd >badcheck.txt || GOTO error
FC badcheck.txt badresults.txt || GOTO error
DEL badcheck.txt
)

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
