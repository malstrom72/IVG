@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
CD /D "%~dp0.."
REM Copies MSVC's AddressSanitizer runtime DLL into output\, so that executables built there with /fsanitize=address run
REM outside a Visual Studio prompt. Finds Visual Studio the way BuildCpp.cmd does (which is not to be edited).
SET "pfpath=%ProgramFiles(x86)%"
IF NOT DEFINED pfpath SET "pfpath=%ProgramFiles%"
SET "vswhere=%pfpath%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT EXIST "%vswhere%" (
	ECHO Could not find vswhere.exe
	EXIT /B 1
)
FOR /F "usebackq tokens=*" %%a IN (`"%vswhere%" -latest -products * -property installationPath`) DO SET "vsInstallPath=%%a"
SET /P toolsVersion=<"%vsInstallPath%\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt"
SET "dll=%vsInstallPath%\VC\Tools\MSVC\%toolsVersion%\bin\Hostx64\x64\clang_rt.asan_dynamic-x86_64.dll"
IF NOT EXIST "%dll%" (
	ECHO Could not find %dll%
	EXIT /B 1
)
COPY /Y "%dll%" output\ >NUL || EXIT /B 1
