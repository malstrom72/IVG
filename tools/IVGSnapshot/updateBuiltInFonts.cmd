@ECHO OFF
CD /D "%~dp0"
node .\updateBuiltInFonts.node.js || GOTO error
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
