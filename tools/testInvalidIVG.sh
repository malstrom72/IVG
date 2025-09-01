#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

EXE=${1:-../output/InvalidIVGTest}

for f in ./ivg/invalid/*.ivg; do
		"$EXE" "$f"
done
