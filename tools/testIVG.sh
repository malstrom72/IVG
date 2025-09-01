#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
if [ -z "${1-}" ]; then
	EXE="../output/IVG2PNG"
else
	EXE=$1
fi
FONTS=../fonts
IMAGES=.

tmp=$(mktemp -d)
echo Using temporary dir: "$tmp"

for f in ./ivg/*.ivg; do
	n=${f#./ivg/}
	n=${n%.ivg}
	echo Doing "$n"
	echo
	args=""
	if [ "$n" = "huge" ]; then
		args="--fast"
	fi
	$EXE $args --images "$IMAGES" --fonts "$FONTS" "$f" "$tmp/$n.png"
	if [ "$UPDATE" -eq 1 ]; then
		cp "$tmp/$n.png" "./png/$n.png"
	else
		cmp "$tmp/$n.png" "./png/$n.png"
	fi
	echo
	echo
done
rm -rf "$tmp"/*.png
rmdir "$tmp"
