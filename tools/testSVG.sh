#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests/svg

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
EXE=${1:-../../output/IVG2PNG}
FONTS=../../fonts
TMP=$(mktemp -d)

echo Using temporary dir: "$TMP"

for NAME in circle rect ellipse line path group color-names stroke-fill viewbox multi-path polygon polyline units \
				percentage transform skew matrix gradient gradient-stops gradient-radial gradient-transform defs-use opacity \
				text text-stroke resvg_tests_shapes_rect_em-values resvg_tests_shapes_rect_vw-and-vh-values \
				resvg_tests_painting_color_inherit resvg_tests_masking_clipPath_clipPathUnits=objectBoundingBox \
				resvg_tests_painting_marker_marker-on-line resvg_tests_painting_stroke-dasharray_ws-separator \
				resvg_tests_painting_stroke-dasharray_on-a-circle resvg_tests_painting_stroke-dashoffset_default \
				resvg_tests_painting_stroke-dashoffset_negative-value blossom blossomCSS blossomStyles; do
		echo Testing "$NAME"
	node ../../tools/svg2ivg.js "supported/$NAME.svg" 500,500 | tail -n +2 > "$TMP/$NAME.ivg"
	$EXE --fonts "$FONTS" --background white "$TMP/$NAME.ivg" "$TMP/$NAME.png"
	if [ "$UPDATE" -eq 1 ]; then
		cp "$TMP/$NAME.ivg" "supported/$NAME.ivg"
		cp "$TMP/$NAME.png" "supported/$NAME.png"
	else
		cmp "$TMP/$NAME.ivg" "supported/$NAME.ivg"
		if [ ! -f "supported/$NAME.png" ]; then
			echo "Missing golden PNG: supported/$NAME.png" >&2
			exit 1
		fi
		cmp "$TMP/$NAME.png" "supported/$NAME.png"
	fi
		echo
		echo
done
rm -rf "$TMP"
