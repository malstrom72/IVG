@ECHO OFF
SETLOCAL
CD /D "%~dp0\.."

	FOR /F "delims=" %%i IN ('uname 2^>NUL') DO SET "sys=%%i"
	IF /I NOT "%sys%"=="Darwin" GOTO :EOF
	FOR /F "delims=" %%i IN ('xcrun --sdk macosx --show-sdk-path 2^>NUL') DO SET "sdk=%%i"
	IF "%sdk%"=="" GOTO :EOF
	SET "SDKROOT=%sdk%"
	ECHO -isysroot %sdk%
