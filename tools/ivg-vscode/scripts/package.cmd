@ECHO OFF
SETLOCAL

CD /D "%~dp0\.."

CALL scripts\sync-assets.cmd --build
IF ERRORLEVEL 1 GOTO error

npm install --no-audit --no-fund
IF ERRORLEVEL 1 GOTO error

npm run compile
IF ERRORLEVEL 1 GOTO error

WHERE vsce >NUL 2>NUL
IF ERRORLEVEL 1 (
SET "VSCE_BIN=npx"
SET "VSCE_ARGS=vsce package"
) ELSE (
SET "VSCE_BIN=vsce"
SET "VSCE_ARGS=package"
)

"%VSCE_BIN%" %VSCE_ARGS% %*
IF ERRORLEVEL 1 GOTO error

ENDLOCAL
EXIT /b 0

:error
ENDLOCAL
EXIT /b %ERRORLEVEL%
