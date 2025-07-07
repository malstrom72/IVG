#!/bin/bash
set -e -o pipefail

if [ -z "$1" ]; then
	EXE="../output/IVG2PNG"
else
	EXE=$1
fi

tmp=$(mktemp -d)
echo Using temporary dir: "$tmp"

for f in ./ivg/*.ivg; do
	n=${f#./ivg/}
	n=${n%.ivg}
	echo Doing "$n"
	echo
	$EXE "$f" "$tmp/$n.png"
	cmp "$tmp/$n.png" "./png/$n.png"
	echo
	echo
done

echo
echo ALL GOOD!!
echo

rm -rf "$tmp"/*.png
rmdir "$tmp"
