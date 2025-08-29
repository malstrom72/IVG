@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests\svg"

IF "%~1"=="" (
	SET exe=..\..\output\IVG2PNG
) ELSE (
	SET exe=%~1
)
SET fonts=..\..\fonts

SET tempDir=%TEMP%\temp%RANDOM%
ECHO Using temporary dir: %tempDir%
MKDIR %tempDir%

FOR %%n IN (
	circle rect ellipse line path group color-names stroke-fill viewbox multi-path polygon polyline units percentage transform skew matrix gradient gradient-stops gradient-radial gradient-transform defs-use opacity text text-stroke resvg_tests_shapes_rect_em-values resvg_tests_shapes_rect_vw-and-vh-values resvg_tests_painting_color_inherit "resvg_tests_masking_clipPath_clipPathUnits=objectBoundingBox" resvg_tests_painting_marker_marker-on-line
) DO (
	ECHO Testing %%~n
	node ..\..\tools\svg2ivg.js "supported/%%~n.svg" 500,500 > "%tempDir%\%%~n.tmp" || GOTO error
	more +1 "%tempDir%\%%~n.tmp" > "%tempDir%\%%~n.ivg"
	DEL "%tempDir%\%%~n.tmp"
	fc "%tempDir%\%%~n.ivg" "supported\%%~n.ivg" || GOTO error
	IF EXIST "supported\%%~n.png" (
		%exe% --fonts %fonts% --background white "%tempDir%\%%~n.ivg" "%tempDir%\%%~n.png" || GOTO error
	)
	ECHO.
	ECHO.
)
ECHO.
ECHO ALL GOOD!!
ECHO.
DEL /q "%tempDir%\*" >NUL 2>NUL
RMDIR "%tempDir%"
EXIT /b 0
:error
ECHO Error %ERRORLEVEL%
DEL /q "%tempDir%\*" >NUL 2>NUL
RMDIR "%tempDir%"
EXIT /b %ERRORLEVEL%
