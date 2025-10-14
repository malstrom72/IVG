@ECHO OFF
REM Regenerate Ace highlight rules from .tmLanguage and build a bundled mode (mode-ivg.js).
REM This script is IVGFiddle-specific and always runs dryice to produce a self-contained mode.
REM Usage: set ACE_REF=v1.43.3 & tools\ivgfiddle\updateAceSyntax.cmd

CD /D "%~dp0\..\.."

SET ACE_DIR=externals\ace
IF "%ACE_REF%"=="" SET ACE_REF=v1.43.3
SET DEST_DIR=tools\ivgfiddle\src\ace
SET TM_IVG=tools\grammars\ivg.tmLanguage
SET TM_IMPD=tools\grammars\impd.tmLanguage

WHERE git  >NUL 2>NUL || (ECHO Missing required tool: git & GOTO error)
WHERE node >NUL 2>NUL || (ECHO Missing required tool: node & GOTO error)
WHERE npm  >NUL 2>NUL || (ECHO Missing required tool: npm  & GOTO error)

IF NOT EXIST "%ACE_DIR%\.git" (
  ECHO Cloning Ace toolchain into %ACE_DIR% ...
  git clone --depth 1 https://github.com/ajaxorg/ace.git "%ACE_DIR%" || GOTO error
) ELSE (
  ECHO Ace already present at %ACE_DIR% (skipping clone).
)

PUSHD "%ACE_DIR%"
git fetch --tags --depth 1 >NUL 2>&1
git checkout -q "%ACE_REF%" >NUL 2>&1 || git checkout -q "tags/%ACE_REF%" >NUL 2>&1
IF ERRORLEVEL 1 (
  ECHO Warning: Could not checkout "%ACE_REF%"; using current HEAD of Ace.
) ELSE (
  ECHO Using Ace converter at ref: %ACE_REF%
)
ECHO Installing converter deps (amd-loader, plist, cson) ...
CALL npm i --no-audit --no-fund amd-loader plist cson >NUL 2>&1
POPD

PUSHD "%ACE_DIR%"
node -e "require('amd-loader');require('plist');require('cson');" >NUL 2>NUL || (
  POPD
  ECHO Converter dependencies are missing or could not be loaded.
  ECHO Re-run when online to regenerate syntax.
  GOTO error
)

ECHO Converting tmLanguage → Ace rules (ivg/impd) ...
node "%ACE_DIR%\tool\tmlanguage.js" "%TM_IMPD%" "%TM_IVG%" || GOTO error

ECHO Building Ace bundle with dryice (-m -nc) ...
CALL node "%ACE_DIR%\Makefile.dryice.js" -m -nc >NUL 2>&1 || GOTO error

IF NOT EXIST "%ACE_DIR%\build\src-min-noconflict\mode-ivg.js" (
  ECHO Error: dryice bundle not found at %ACE_DIR%\build\src-min-noconflict\mode-ivg.js
  GOTO error
)

IF NOT EXIST "%DEST_DIR%" MKDIR "%DEST_DIR%"
COPY /Y "%ACE_DIR%\build\src-min-noconflict\mode-ivg.js" "%DEST_DIR%\mode-ivg.js" >NUL || GOTO error
ECHO Bundled mode updated: %DEST_DIR%\mode-ivg.js
ECHO Note: Normal builds do not require this step; this is only for refreshing IVGFiddle syntax.

EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
