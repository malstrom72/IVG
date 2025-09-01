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
	args=""
	if [ "$n" = "huge" ]; then
		args="--fast"
	fi
	"$EXE" $args --images "$IMAGES" --fonts "$FONTS" "$f" "png/$n.png"
	echo
	echo
done

