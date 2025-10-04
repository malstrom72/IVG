@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

SET "SCRIPT_DIR=%~dp0"
PUSHD "%SCRIPT_DIR%..\..\.."
SET "REPO_ROOT=%CD%"
SET "EXT_DIR=%REPO_ROOT%\tools\ivg-vscode"
SET "OUTPUT_DIR=%REPO_ROOT%\tools\ivgfiddle\output"

SET "BUILD=0"
IF /I "%~1"=="--build" (
	SET "BUILD=1"
	SHIFT
)

IF NOT "%~1"=="" (
	ECHO Usage: %~n0 [--build]
	POPD
	EXIT /B 1
)

IF "%BUILD%"=="1" (
	CALL tools\ivgfiddle\buildIVGFiddle.cmd
	IF ERRORLEVEL 1 (
		POPD
		EXIT /B %ERRORLEVEL%
	)
)

IF NOT EXIST "%OUTPUT_DIR%\ivgfiddle.html" (
	ECHO Expected %OUTPUT_DIR%\ivgfiddle.html but it was not found.
	POPD
	EXIT /B 1
)

IF NOT EXIST "%EXT_DIR%\media" (
	MKDIR "%EXT_DIR%\media"
)

ROBOCOPY "%OUTPUT_DIR%" "%EXT_DIR%\media" /MIR >NUL
SET "RC=%ERRORLEVEL%"
IF %RC% GEQ 8 (
	POPD
	EXIT /B %RC%
)

ECHO Synchronized assets into %EXT_DIR%\media

POPD
EXIT /B 0
