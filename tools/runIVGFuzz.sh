#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..
# Fuzzes output/IVGFuzz (built by tools/buildIVGFuzz.sh) until stopped with Ctrl-C, one process per logical CPU,
# starting from every .ivg in the repository. Crashes, out-of-memory and timeouts are saved in output/fuzzArtifacts
# and fuzzing goes on. New inputs build up in output/fuzzCorpus, so a later run continues where this one stopped.
# Extra arguments go to libFuzzer and override the defaults, e.g. runIVGFuzz.sh -fork=8 -max_total_time=3600
bash ./tools/collectFuzzCorpus.sh output/fuzzSeeds
mkdir -p output/fuzzCorpus output/fuzzArtifacts
./output/IVGFuzz -fork="$(getconf _NPROCESSORS_ONLN)" -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -rss_limit_mb=4096 \
		-timeout=30 -artifact_prefix=output/fuzzArtifacts/ "$@" output/fuzzCorpus output/fuzzSeeds
