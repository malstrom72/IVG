@ECHO OFF
CD /D "%~dp0"

CALL .\tools\buildAndTest.cmd beta x64 nosimd || GOTO error
CALL .\tools\buildAndTest.cmd release x64 nosimd || GOTO error

CALL .\tools\buildAndTest.cmd beta x64 simd || GOTO error
CALL .\tools\buildAndTest.cmd release x64 simd || GOTO error

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
