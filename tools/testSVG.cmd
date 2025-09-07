@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\..\tests\svg"

SET update=0
IF /I "%~1"=="update" (
    SET update=1
    SHIFT
)
IF "%~1"=="" (
    SET exe=..\..\output\IVG2PNG
) ELSE (
    SET exe=%~1
)
SET fonts=..\..\fonts

SET tempDir=%TEMP%\temp%RANDOM%
ECHO Using temporary dir: %tempDir%
MKDIR %tempDir%

SET fail=0
FOR %%n IN (
    circle rect ellipse line path group color-names stroke-fill viewbox multi-path polygon polyline units percentage transform skew matrix gradient gradient-stops gradient-radial gradient-transform defs-use opacity text text-stroke resvg_tests_shapes_rect_em-values resvg_tests_shapes_rect_vw-and-vh-values resvg_tests_painting_color_inherit "resvg_tests_masking_clipPath_clipPathUnits=objectBoundingBox" resvg_tests_painting_marker_marker-on-line
) DO (
    ECHO Testing %%~n
    node ..\..\tools\svg2ivg.js "supported/%%~n.svg" 500,500 > "%tempDir%\%%~n.tmp"
    IF ERRORLEVEL 1 SET fail=1
    more +1 "%tempDir%\%%~n.tmp" > "%tempDir%\%%~n.ivg"
    DEL "%tempDir%\%%~n.tmp"
    %exe% --fonts %fonts% --background white "%tempDir%\%%~n.ivg" "%tempDir%\%%~n.png"
    IF ERRORLEVEL 1 SET fail=1
    IF %update%==1 (
        COPY "%tempDir%\%%~n.ivg" "supported\%%~n.ivg" >NUL
        COPY "%tempDir%\%%~n.png" "supported\%%~n.png" >NUL
    ) ELSE (
        fc "%tempDir%\%%~n.ivg" "supported\%%~n.ivg"
        IF ERRORLEVEL 1 SET fail=1
        IF EXIST "supported\%%~n.png" (
            fc "%tempDir%\%%~n.png" "supported\%%~n.png"
            IF ERRORLEVEL 1 SET fail=1
        ) ELSE (
            ECHO Missing golden PNG: supported\%%~n.png
            SET fail=1
        )
    )
    ECHO.
    ECHO.
)
DEL /q "%tempDir%\*" >NUL 2>NUL
RMDIR "%tempDir%"
EXIT /b %fail%
