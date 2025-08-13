@ECHO OFF
CD /D "%~dp0"
SETLOCAL ENABLEDELAYEDEXPANSION

FOR /R tests\svg %%F IN (*.svg) DO (
	SET "base=%%~nF"
	IF "!base:~0,12!"=="resvg_tests_" (
		SET "rest=!base:~12!"
		SET "rel=!rest:_=\!"
		SET "src=externals\resvgTests\!rel!.png"
		IF EXIST "!src!" (
			COPY /Y "!src!" "%%~dpF%%~nF.png" >NUL
		) ELSE (
			>&2 ECHO missing "!src!"
		)
	)
)
EXIT /b 0
