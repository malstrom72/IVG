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

COPY /Y "%OUTPUT_DIR%\rasterizeIVG.js" "%EXT_DIR%\media\rasterizeIVG.js" >NUL
IF ERRORLEVEL 1 (
	POPD
	EXIT /B 1
)

REM Only docs is mirrored, and only when the build produced it. Everything else under
REM media is checked in and must survive a sync.
IF NOT EXIST "%OUTPUT_DIR%\docs" GOTO :docsDone
ROBOCOPY "%OUTPUT_DIR%\docs" "%EXT_DIR%\media\docs" /MIR >NUL
IF ERRORLEVEL 8 (
	POPD
	EXIT /B 1
)
:docsDone

ECHO Synchronized assets into %EXT_DIR%\media

POPD
EXIT /B 0
