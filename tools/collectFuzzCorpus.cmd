@echo off
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
CD /D "%~dp0.."

IF "%~1"=="" (
	ECHO Usage: collectFuzzCorpus.cmd ^<target-dir^> >&2
	EXIT /b 1
)

SET "target=%~1"
IF NOT EXIST "%target%" MKDIR "%target%"
FOR %%A IN ("%target%") DO SET "abs_target=%%~fA"

FOR /R %%F IN (*.ivg) DO (
	SET "file=%%~fF"
	SET "skip="
	IF NOT "!file:%abs_target%\\=!"=="!file!" SET "skip=1"
	IF NOT "!file:output\\=!"=="!file!" SET "skip=1"
	IF NOT "!file:\.=!"=="!file!" SET "skip=1"
	IF NOT DEFINED skip (
		COPY /Y "%%F" "!abs_target!\%%~nxF" >NUL
	)
)
