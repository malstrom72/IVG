@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
SETLOCAL

IF "%~1"=="" (
	ECHO.
	ECHO UninstallPika ^<installation path^>
	ECHO.
	ECHO To uninstall from C:\WINDOWS, open a Command Prompt with "Run as administrator", go to this folder and type:
	ECHO.
	ECHO UninstallPika.cmd C:\WINDOWS
	EXIT /B 1
)
DEL %1\Pika.cmd
DEL %1\PikaCmd.exe
DEL %1\systools.pika
