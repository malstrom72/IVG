#!/bin/bash
set -e -o pipefail

if [ -z "$1" ]; then
	EXE="../output/IVG2PNG"
else
	EXE=$1
fi

for f in ./ivg/*.ivg; do
	n=${f#./ivg/}
	n=${n%.ivg}
	echo Doing "$n"
	echo
	$EXE "$f" "./png/$n.png"
	echo
	echo

done

