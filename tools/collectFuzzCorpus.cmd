@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
CD /D "%~dp0\.."

IF "%~1"=="" GOTO usage
IF NOT "%~2"=="" GOTO usage

IF NOT EXIST "%~1" MKDIR "%~1" || EXIT /b 1
FOR %%A IN ("%~1") DO SET "target=%%~fA"
SET "root=%CD%"

REM Same files as collectFuzzCorpus.sh: every .ivg except those under .\output, a dot directory or the target itself.
FOR /R %%F IN (*.ivg) DO (
	SET "file=%%~fF"
	SET "rel=!file:%root%\=!"
	SET "skip="
	REM "*.ivg" also matches 8.3 short names, so .ivgfont files come through too.
	IF /I NOT "%%~xF"==".ivg" SET "skip=1"
	IF /I "!rel:~0,7!"=="output\" SET "skip=1"
	IF "!rel:~0,1!"=="." SET "skip=1"
	IF NOT "!rel:\.=!"=="!rel!" SET "skip=1"
	IF NOT "!file:%target%\=!"=="!file!" SET "skip=1"
	IF NOT DEFINED skip (
		COPY /Y "%%F" "%target%\%%~nxF" >NUL || EXIT /b 1
	)
)
EXIT /b 0

:usage
ECHO Usage: tools\collectFuzzCorpus.cmd ^<target-dir^> 1>&2
EXIT /b 1
