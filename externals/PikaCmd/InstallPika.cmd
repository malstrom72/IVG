@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
SETLOCAL

IF "%~1"=="" (
	ECHO.
	ECHO InstallPika ^<target path^>
	ECHO.
	ECHO To install into C:\WINDOWS, open a Command Prompt with "Run as administrator", go to this folder and type:
	ECHO.
	ECHO InstallPika.cmd C:\WINDOWS
	EXIT /B 1
)
COPY Pika.cmd %1\ || GOTO error
COPY PikaCmd.exe %1\ || GOTO error
COPY systools.pika %1\ || GOTO error
ASSOC .pika=PikaScript
FTYPE PikaScript=%1\Pika.cmd %%1 %%*
EXIT /B 0

:error
ECHO Error %ERRORLEVEL%
EXIT /B %ERRORLEVEL%
