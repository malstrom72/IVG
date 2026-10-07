@ECHO OFF
SETLOCAL
CD /D "%~dp0.."
REM Fuzzes output\IVGFuzz (built by tools\buildIVGFuzz.cmd) until stopped with Ctrl-C, one process per logical CPU,
REM starting from every .ivg in the repository. Crashes, out-of-memory and timeouts are saved in output\fuzzArtifacts
REM and fuzzing goes on. New inputs build up in output\fuzzCorpus, so a later run continues where this one stopped.
REM An input counts as a timeout after 120 s, about 3 s of work in a release build, since MSVC's fuzzer
REM instrumentation is much slower than clang's (runIVGFuzz.sh allows 30 s).
REM Extra arguments go to libFuzzer and override the defaults, e.g. runIVGFuzz.cmd -fork=8 -max_total_time=3600
CALL tools\collectFuzzCorpus.cmd output\fuzzSeeds || EXIT /B 1
IF NOT EXIST output\fuzzCorpus MKDIR output\fuzzCorpus
IF NOT EXIST output\fuzzArtifacts MKDIR output\fuzzArtifacts
output\IVGFuzz.exe -fork=%NUMBER_OF_PROCESSORS% -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -rss_limit_mb=2048 -timeout=120 -artifact_prefix=output\fuzzArtifacts\ %* output\fuzzCorpus output\fuzzSeeds || EXIT /B 1
