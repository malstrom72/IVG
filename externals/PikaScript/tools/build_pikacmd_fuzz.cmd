@ECHO OFF
SETLOCAL
CD /D "%~dp0\.."
IF NOT EXIST output MKDIR output
SET "CPP_OPTIONS=/std:c++14 /fsanitize=fuzzer,address"
CALL tools\PikaCmd\SourceDistribution\BuildCpp.cmd debug x64 output\PikaCmdFuzz.exe ^
		/D LIBFUZZ /D PLATFORM_STRING=WINDOWS /I src ^
		tools\PikaCmd\PikaCmd.cpp tools\PikaCmd\BuiltIns.cpp src\PikaScript.cpp src\QStrings.cpp || GOTO error
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
