#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Replays the committed fuzz corpus, tests/fuzz/IVGFuzzCorpus.tar.gz, through output/IVGFuzzReplay, which build.sh
# builds. It is too slow for every build, so CI runs it once after the build, and it is run by hand before a freeze.
# The archive is unpacked into an emptied folder, so no input left from an earlier archive is replayed.
if [ ! -e ./output/IVGFuzzReplay ]; then
	echo "output/IVGFuzzReplay is missing; run build.sh first" >&2
	exit 1
fi
rm -rf ./output/fuzzCorpusReplay
mkdir -p ./output/fuzzCorpusReplay
tar -xzf ./tests/fuzz/IVGFuzzCorpus.tar.gz -C ./output/fuzzCorpusReplay
find ./output/fuzzCorpusReplay -type f | ./output/IVGFuzzReplay -
