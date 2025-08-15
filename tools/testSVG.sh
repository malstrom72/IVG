#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests/svg

EXE=../../output/IVG2PNG
FONTS=../../fonts
if [ $# -gt 0 ]; then
	EXE=$1
fi
TMP=$(mktemp -d)

echo Using temporary dir: "$TMP"

for NAME in circle rect ellipse line path group color-names stroke-fill viewbox multi-path polygon polyline units \
		percentage transform skew matrix gradient gradient-stops gradient-radial gradient-transform defs-use opacity \
		text text-stroke; do
echo Testing "$NAME"
node ../../tools/svg2ivg/svg2ivg.js "supported/$NAME.svg" 500,500 | tail -n +2 > "$TMP/$NAME.ivg"
cmp "$TMP/$NAME.ivg" "supported/$NAME.ivg"
if [ -f "supported/$NAME.png" ]; then
$EXE --fonts "$FONTS" --background white "$TMP/$NAME.ivg" "$TMP/$NAME.png"
fi
echo
echo
done

echo
echo ALL GOOD!!
echo

rm -rf "$TMP"
