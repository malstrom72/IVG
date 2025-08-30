@ECHO OFF
CD /D "%~dp0"

CALL .\tools\buildAndTest.cmd beta x64 nosimd || GOTO error
CALL .\tools\buildAndTest.cmd release x64 nosimd || GOTO error

CALL .\tools\buildAndTest.cmd beta x64 simd || GOTO error
CALL .\tools\buildAndTest.cmd release x64 simd || GOTO error

WHERE emcc >NUL 2>NUL
IF ERRORLEVEL 1 (
    ECHO Warning: skipping ivgfiddle build; requires Emscripten
) ELSE (
    CALL .\tools\ivgfiddle\buildIVGFiddle.cmd || GOTO error
)

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
