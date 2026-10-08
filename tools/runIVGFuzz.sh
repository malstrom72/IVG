#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Fuzzes output/IVGFuzz (built by tools/buildIVGFuzz.sh) until stopped with Ctrl-C, one process per logical CPU,
# starting from every .ivg in the repository. Crashes, out-of-memory and timeouts are saved in output/fuzzArtifacts
# and fuzzing goes on. New inputs build up in output/fuzzCorpus, so a later run continues where this one stopped.
# An input counts as a timeout after 30 s, about 3 s of work in a release build given the instrumentation's cost
# with clang. A longer limit lets slow inputs into the corpus, where they hold up every later job.
# Extra arguments go to libFuzzer and override the defaults, e.g. runIVGFuzz.sh -fork=8 -max_total_time=3600
bash ./tools/collectFuzzCorpus.sh output/fuzzSeeds
mkdir -p output/fuzzCorpus output/fuzzArtifacts
# On macOS libFuzzer's prebuilt library is not built with AddressSanitizer but shares std::vector code with the target,
# so the vector annotations disagree and fork mode stops at a false container overflow in libFuzzer's own code. Real
# heap overflows are still caught without that check. An ASAN_OPTIONS of your own is left as it is.
if [[ "$(uname -s)" == "Darwin" ]]; then
	export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_container_overflow=0}"
fi
./output/IVGFuzz -fork="$(getconf _NPROCESSORS_ONLN)" -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -rss_limit_mb=2048 \
		-timeout=30 -artifact_prefix=output/fuzzArtifacts/ "$@" output/fuzzCorpus output/fuzzSeeds
