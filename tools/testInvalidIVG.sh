#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
EXE=${1:-../output/InvalidIVGTest}

if [ "$UPDATE" -eq 1 ]; then
	for f in ./ivg/invalid/*.ivg; do
		"$EXE" "$f"
	done | tee invalidIVGResults.txt
else
	for f in ./ivg/invalid/*.ivg; do
		"$EXE" "$f"
	done
fi
