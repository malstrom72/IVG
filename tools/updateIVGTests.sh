#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

EXE=${1:-../output/IVG2PNG}
FONTS=../fonts
IMAGES=.

for f in ivg/*.ivg; do
		n=${f#ivg/}
		n=${n%.ivg}
		echo Doing "$n"
		echo
	"$EXE" --fonts "$FONTS" --images "$IMAGES" "$f" "png/$n.png"
		echo
		echo
done

