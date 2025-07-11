@echo off
CD /D "%~dp0\..\tests"
echo.
echo ### GOOD TESTS ###

echo.
.\Debug\ImpDTest <goodTests.impd >goodCheck.txt
fc goodcheck.txt goodresults.txt
del goodcheck.txt
echo.
echo ### BAD TESTS ###

echo.
.\Debug\ImpDTest <badTests.impd >badcheck.txt
fc badcheck.txt badresults.txt
del badcheck.txt
