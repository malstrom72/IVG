@ECHO OFF
CD /D "%~dp0\..\.."

SETLOCAL ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

SET CSOURCES=
FOR %%f IN (
png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c ^
pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c ^
pngwrite.c pngwtran.c pngwutil.c
) DO (
SET "CSOURCES=!CSOURCES! externals\libpng\%%f"
)
FOR %%f IN (
adler32.c compress.c crc32.c deflate.c infback.c inffast.c ^
inflate.c inftrees.c trees.c uncompr.c zutil.c
) DO (
SET "CSOURCES=!CSOURCES! externals\zlib\%%f"
)

IF NOT EXIST .\output MD .\output
CALL .\tools\BuildCpp.cmd beta x64 .\output\IVGSnapshot -ffp-contract=off "-DNUXPIXELS_SIMD=0" ^
        /I"." /I"externals" /I"externals\libpng" /I"externals\zlib" ^
        .\tools\IVGSnapshot\IVGSnapshot.cpp .\src\IVG.cpp .\src\IMPD.cpp ^
        .\externals\NuX\NuXThreads.cpp .\externals\NuX\NuXThreadsPosix.cpp ^
        .\externals\NuX\NuXFiles.cpp .\externals\NuX\NuXFilesPosix.cpp ^
        .\externals\NuX\NuXPixels.cpp %CSOURCES%
EXIT /B %ERRORLEVEL%
