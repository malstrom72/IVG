@ECHO OFF
CD /D "%~dp0"

CALL .\tools\buildAndTest.cmd beta native nosimd || GOTO error
CALL .\tools\buildAndTest.cmd release native nosimd || GOTO error

CALL .\tools\buildAndTest.cmd beta native simd || GOTO error
CALL .\tools\buildAndTest.cmd release native simd || GOTO error

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
