@ECHO OFF
SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION
CD /D "%~dp0\.."

IF "%CPP_TARGET%"=="" SET CPP_TARGET=release
IF "%CPP_MODEL%"=="" SET CPP_MODEL=x64
IF "%C_OPTIONS%"=="" SET C_OPTIONS=
IF "%CPP_OPTIONS%"=="" SET CPP_OPTIONS=

REM Extract /std or -std flags
SET CPP_STD=
SET tmp=
FOR %%i IN (%CPP_OPTIONS%) DO (
	SET "opt=%%i"
	IF /I "!opt:~0,5!"=="-std=" (
		SET CPP_STD=%%i
	) ELSE IF /I "!opt:~0,5!"=="/std:" (
		SET CPP_STD=%%i
	) ELSE (
		SET "tmp=!tmp! %%i"
	)
)
SET "CPP_OPTIONS=%tmp%"
SET tmp=
SET C_STD=
FOR %%i IN (%C_OPTIONS%) DO (
	SET "opt=%%i"
	IF /I "!opt:~0,5!"=="-std=" (
		SET C_STD=%%i
	) ELSE IF /I "!opt:~0,5!"=="/std:" (
		SET C_STD=%%i
	) ELSE (
		SET "tmp=!tmp! %%i"
	)
)
SET "C_OPTIONS=%tmp%"

IF "%~1"=="debug" (
	SET CPP_TARGET=debug
	SHIFT
) ELSE IF "%~1"=="beta" (
	SET CPP_TARGET=beta
	SHIFT
) ELSE IF "%~1"=="release" (
	SET CPP_TARGET=release
	SHIFT
)

IF "%~1"=="x86" (
	SET CPP_MODEL=x86
	SHIFT
) ELSE IF "%~1"=="x64" (
	SET CPP_MODEL=x64
	SHIFT
) ELSE IF "%~1"=="arm64" (
	SET CPP_MODEL=arm64
	SHIFT
)

IF "%CPP_TARGET%"=="debug" (
	SET C_OPTIONS=/Od /MTd /GS /Zi /D DEBUG %C_OPTIONS%
	SET CPP_OPTIONS=/Od /MTd /GS /Zi /D DEBUG %CPP_OPTIONS%
) ELSE IF "%CPP_TARGET%"=="beta" (
	SET C_OPTIONS=/O2 /GL /MTd /GS /Zi /D DEBUG %C_OPTIONS%
	SET CPP_OPTIONS=/O2 /GL /MTd /GS /Zi /D DEBUG %CPP_OPTIONS%
) ELSE IF "%CPP_TARGET%"=="release" (
	SET C_OPTIONS=/O2 /GL /MT /GS- /D NDEBUG %C_OPTIONS%
	SET CPP_OPTIONS=/O2 /GL /MT /GS- /D NDEBUG %CPP_OPTIONS%
) ELSE (
	ECHO Unrecognized CPP_TARGET %CPP_TARGET%
	EXIT /B 1
)

IF "%CPP_MODEL%"=="arm64" (
	SET vcvarsConfig=arm64
) ELSE IF "%CPP_MODEL%"=="x64" (
	SET vcvarsConfig=amd64
) ELSE IF "%CPP_MODEL%"=="x86" (
	SET C_OPTIONS=/arch:SSE2 %C_OPTIONS%
	SET CPP_OPTIONS=/arch:SSE2 %CPP_OPTIONS%
	SET vcvarsConfig=x86
) ELSE (
	ECHO Unrecognized CPP_MODEL %CPP_MODEL%
	EXIT /B 1
)

SET output=%1
SET name=%~n1
SHIFT

SET common=/W3 /EHsc /D "WIN32" /D "_CONSOLE" /D "_CRT_SECURE_NO_WARNINGS" /D "_SCL_SECURE_NO_WARNINGS"
SET C_OPTIONS=%common% %C_OPTIONS%
SET CPP_OPTIONS=%common% %CPP_OPTIONS%

IF "%name%"=="" (
	ECHO BuildCpp [debug^|beta^|release] [x86^|x64^|arm64] ^<output.exe^> ^<source files and other compiler arguments^>
	ECHO You can also use the environment variables: CPP_MSVC_VERSION, CPP_TARGET, CPP_MODEL, CPP_OPTIONS and C_OPTIONS
	EXIT /B 1
)

SET args=
SET mode=
:argLoop
		IF "%~1"=="" GOTO argLoopEnd
		SET "arg=%~1"
		IF "!arg:~0,1!"=="-" (
				SET "args=!args! !arg!"
		) ELSE IF "!arg:~0,1!"=="/" (
				SET "args=!args! !arg!"
		) ELSE (
				IF /I "!arg:~-2!"==".c" (
						IF /I NOT "!mode!"=="c" (
								SET "args=!args! %C_OPTIONS% %C_STD%"
								SET mode=c
						)
				) ELSE (
						IF /I NOT "!mode!"=="cpp" (
								SET "args=!args! %CPP_OPTIONS% %CPP_STD%"
								SET mode=cpp
						)
				)
				SET "args=!args! !arg!"
		)
		SHIFT
GOTO argLoop
:argLoopEnd
IF /I NOT "%mode%"=="cpp" SET "args=!args! %CPP_OPTIONS% %CPP_STD%"

SET pfpath=%ProgramFiles(x86)%
IF NOT DEFINED pfpath SET pfpath=%ProgramFiles%

IF NOT DEFINED VCINSTALLDIR (
	IF EXIST "%pfpath%\Microsoft Visual Studio\Installer\vswhere.exe" (
	IF "%CPP_MSVC_VERSION%"=="" (
		SET "range= "
	) else (
		SET "range=-version [%CPP_MSVC_VERSION%,%CPP_MSVC_VERSION%]"
	)
	for /f "usebackq tokens=*" %%a in (`"%pfpath%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -legacy !range! -products * -property installationPath`) do set vsInstallPath=%%a
	IF EXIST "!vsInstallPath!\VC\Auxiliary\Build\vcvarsall.bat" (
		CALL "!vsInstallPath!\VC\Auxiliary\Build\vcvarsall.bat" %vcvarsConfig% >NUL
		GOTO foundTools
	)
	IF EXIST "!vsInstallPath!\VC\vcvarsall.bat" (
		CALL "!vsInstallPath!\VC\vcvarsall.bat" %vcvarsConfig% >NUL
		GOTO foundTools
	)
	ECHO Could not find Visual C++ in one of the standard paths.
	) else (
	IF "%CPP_MSVC_VERSION%"=="" (
		FOR /L %%v IN (14,-1,9) DO (
		IF EXIST "%pfpath%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" (
			SET CPP_MSVC_VERSION=%%v
			CALL "%pfpath%\Microsoft Visual Studio %%v.0\VC\vcvarsall.bat" %vcvarsConfig% >NUL
			GOTO foundTools
		)
		)
		ECHO Could not find Visual C++ in one of the standard paths.
	) ELSE (
		IF EXIST "%pfpath%\Microsoft Visual Studio %CPP_MSVC_VERSION%.0\VC\vcvarsall.bat" (
		CALL "%pfpath%\Microsoft Visual Studio %CPP_MSVC_VERSION%.0\VC\vcvarsall.bat" %vcvarsConfig% >NUL
		GOTO foundTools
		)
		ECHO Could not find Visual C++ version %CPP_MSVC_VERSION% in the standard path.
	)
	)
	ECHO Manually run vcvarsall.bat first or run this batch from a Visual Studio command line.
	EXIT /B 1
)
:foundTools

SET temppath=%TEMP:"=%\%name%_%RANDOM%
MKDIR "%temppath%" >NUL 2>&1
ECHO Compiling %name% %CPP_TARGET% %CPP_MODEL% using %VCINSTALLDIR%
ECHO %args% /Fe%output%
ECHO.
cl /errorReport:queue /Fo"%temppath%\\" /Fe"%output%" %args% >"%temppath%\buildlog.txt"
IF ERRORLEVEL 1 (
	TYPE "%temppath%\buildlog.txt"
	ECHO Compilation of %name% failed
	DEL /Q "%temppath%\*" >NUL 2>&1
	RMDIR /Q "%temppath%" >NUL 2>&1
	EXIT /B 1
) ELSE (
	ECHO Compiled %name%
	DEL /Q "%temppath%\*" >NUL 2>&1
	RMDIR /Q "%temppath%\" >NUL 2>&1

	EXIT /B 0
)
