@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
REM Replays the committed fuzz corpus, tests\fuzz\IVGFuzzCorpus.tar.gz, and the crash inputs through
REM output\IVGFuzzReplay, which build.cmd builds. It is too slow for every build, so CI runs it once after the build,
REM and it is run by hand before a freeze. The archive is unpacked into an emptied folder, so no input left from an
REM earlier archive is replayed.
REM Usage: replayFuzzCorpus.cmd [valgrind] (valgrind is Linux only, see replayFuzzCorpus.sh)
IF "%~1"=="valgrind" (
	ECHO Valgrind runs on Linux only: use replayFuzzCorpus.sh there.
	EXIT /B 1
)
IF NOT "%~1"=="" (
	ECHO Usage: replayFuzzCorpus.cmd [valgrind]
	EXIT /B 1
)
IF NOT EXIST output\IVGFuzzReplay.exe (
	ECHO output\IVGFuzzReplay.exe is missing; run build.cmd first
	EXIT /B 1
)
IF EXIST output\fuzzCorpusReplay RMDIR /S /Q output\fuzzCorpusReplay
MKDIR output\fuzzCorpusReplay
tar -xzf tests\fuzz\IVGFuzzCorpus.tar.gz -C output\fuzzCorpusReplay || EXIT /B 1
DIR /B /S /A-D tests\fuzz\IVGFuzzCrashes output\fuzzCorpusReplay | output\IVGFuzzReplay.exe - || EXIT /B 1
EXIT /B 0
