#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Replays the committed fuzz corpus, tests/fuzz/IVGFuzzCorpus.tar.gz, and the crash inputs through output/IVGFuzzReplay,
# which build.sh builds. It is too slow for every build, so CI runs it once after the build, and it is run by hand
# before a freeze. The archive is unpacked into an emptied folder, so no input left from an earlier archive is replayed.
# Usage: replayFuzzCorpus.sh [valgrind] (valgrind runs the replay under Valgrind's memcheck, for reads of uninitialized
# memory, which the sanitizers miss; Linux only)
runner=""
if [[ "${1:-}" == "valgrind" ]]; then
	runner="valgrind -q --error-exitcode=1"
elif [[ $# -gt 0 ]]; then
	echo "Usage: replayFuzzCorpus.sh [valgrind]" >&2
	exit 1
fi
if [ ! -e ./output/IVGFuzzReplay ]; then
	echo "output/IVGFuzzReplay is missing; run build.sh first" >&2
	exit 1
fi
rm -rf ./output/fuzzCorpusReplay
mkdir -p ./output/fuzzCorpusReplay
tar -xzf ./tests/fuzz/IVGFuzzCorpus.tar.gz -C ./output/fuzzCorpusReplay
find ./tests/fuzz/IVGFuzzCrashes ./output/fuzzCorpusReplay -type f | $runner ./output/IVGFuzzReplay -
