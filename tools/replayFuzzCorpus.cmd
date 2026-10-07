@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
REM Replays the committed fuzz corpus, tests\fuzz\IVGFuzzCorpus.tar.gz, through output\IVGFuzzReplay, which build.cmd
REM builds. It is too slow for every build, so CI runs it once after the build, and it is run by hand before a freeze.
REM The archive is unpacked into an emptied folder, so no input left from an earlier archive is replayed.
IF NOT EXIST output\IVGFuzzReplay.exe (
	ECHO output\IVGFuzzReplay.exe is missing; run build.cmd first
	EXIT /B 1
)
IF EXIST output\fuzzCorpusReplay RMDIR /S /Q output\fuzzCorpusReplay
MKDIR output\fuzzCorpusReplay
tar -xzf tests\fuzz\IVGFuzzCorpus.tar.gz -C output\fuzzCorpusReplay || EXIT /B 1
DIR /B /S /A-D output\fuzzCorpusReplay | output\IVGFuzzReplay.exe - || EXIT /B 1
EXIT /B 0
