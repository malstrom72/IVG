@ECHO OFF
CD /D "%~dp0\..\tests"
..\output\IMPDTest <goodTests.impd >goodResults.txt
..\output\IMPDTest <badTests.impd >badResults.txt
