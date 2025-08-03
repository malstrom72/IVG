@ECHO OFF
CD /D "%~dp0\.."
bash tools/build_ivg_fuzz.sh %* || GOTO error
EXIT /B 0
:error
EXIT /B %ERRORLEVEL%
