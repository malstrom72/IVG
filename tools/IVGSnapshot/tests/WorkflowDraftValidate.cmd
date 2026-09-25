@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\..\.."

SET snapshot_tool=.\output\IVGSnapshot.exe
IF NOT EXIST "%snapshot_tool%" (
	ECHO Snapshot tool not built: %snapshot_tool% 1>&2
	EXIT /b 1
)

SET temp_dir=%TEMP%\temp%RANDOM%
MKDIR "%temp_dir%" || EXIT /b 1
SET ivg_file=%temp_dir%\snaptest.ivg
SET output_dir=%temp_dir%\snapshots
SET run_log=%temp_dir%\run.txt
MKDIR "%output_dir%" || GOTO error

CALL :writeIvg no || GOTO error
CALL :runTool || GOTO error

REM The snapshot prefix is the IVG path relative to the root dir, without extension, with "_" doubled.
FOR %%I IN ("%ivg_file%") DO SET snapshot_prefix=%%~nI
SET snapshot_prefix=%snapshot_prefix:_=__%

SET golden_path=%output_dir%\%snapshot_prefix%__unlabeled-1.png
SET old_path=%output_dir%\%snapshot_prefix%__unlabeled-1.png.old
IF NOT EXIST "%old_path%" IF NOT EXIST "%golden_path%" (
	ECHO Draft run did not produce a .png.old artifact. 1>&2
	GOTO fail
)

CALL :writeIvg yes || GOTO error
CALL :runTool || GOTO error
FINDSTR /C:"FAILED" "%run_log%" >NUL && (
	ECHO Initial validation failed. 1>&2
	GOTO fail
)

CALL :runTool || GOTO error
FINDSTR /C:"FAILED" "%run_log%" >NUL && (
	ECHO Second validation run reported a failure. 1>&2
	GOTO fail
)

CALL :writeIvg no || GOTO error
CALL :runTool || GOTO error
FINDSTR /C:"FAILED" "%run_log%" >NUL && (
	ECHO Disabling validation reported a failure. 1>&2
	GOTO fail
)
IF NOT EXIST "%old_path%" (
	ECHO Disabling validation did not regenerate the .png.old draft. 1>&2
	GOTO fail
)
IF EXIST "%golden_path%" (
	ECHO Golden image was not removed when validation was disabled. 1>&2
	GOTO fail
)

CALL :writeIvg yes || GOTO error
CALL :runTool || GOTO error
FINDSTR /C:"FAILED" "%run_log%" >NUL && (
	ECHO Re-enabling validation reported a failure. 1>&2
	GOTO fail
)
IF NOT EXIST "%golden_path%" (
	ECHO Golden image was not restored after re-enabling validation. 1>&2
	GOTO fail
)
IF EXIST "%old_path%" (
	ECHO Draft artifact still present after re-enabling validation. 1>&2
	GOTO fail
)

ECHO Workflow validation completed without diffs.
RMDIR /S /Q "%temp_dir%"
EXIT /b 0

:writeIvg
> "%ivg_file%" (
	ECHO format ivg-3 uses:snapshot-1
	ECHO bounds 0,0,37,37
	ECHO meta snapshot validate:%~1 [
	ECHO 	color=#E74C3C
	ECHO 	highlight=#FFFFFF
	ECHO 	shadow=#000000
	ECHO ]
	ECHO FILL $color
	ECHO ELLIPSE 15,15,14
)
EXIT /b 0

:runTool
"%snapshot_tool%" --snapshot-dir "%output_dir%" --root-dir "%temp_dir%" "%ivg_file%" >"%run_log%"
SET tool_exit=%ERRORLEVEL%
TYPE "%run_log%"
EXIT /b %tool_exit%

:fail
RMDIR /S /Q "%temp_dir%"
EXIT /b 1

:error
SET error_exit=%ERRORLEVEL%
RMDIR /S /Q "%temp_dir%"
EXIT /b %error_exit%
