@ECHO OFF
SETLOCAL ENABLEEXTENSIONS
CD /D "%~dp0\.."

SET outDir=output\svg-samples
SET svgDir=tests\svg

FOR %%c IN (supported unsupported) DO (
	IF NOT EXIST "%outDir%\%%c" MKDIR "%outDir%\%%c"
	FOR %%f IN ("%svgDir%\%%c\*.svg") DO (
		ECHO Converting %%f
		node tools\svg2ivg\svg2ivg.js "%%f" 500,500 | more +2 > "%outDir%\%%c\%%~nf.ivg"
		IF ERRORLEVEL 1 (
			ECHO Failed to convert %%f
		) ELSE (
			output\IVG2PNG --fonts fonts --background white "%outDir%\%%c\%%~nf.ivg" "%outDir%\%%c\%%~nf.png" || ECHO Failed to render %%f
		)
	)
)

EXIT /b 0
