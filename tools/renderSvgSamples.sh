#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

OUT_DIR="output/svg-samples"
SVG_DIR="tests/svg"

for cat in supported unsupported; do
	mkdir -p "$OUT_DIR/$cat"
	for svg in "$SVG_DIR/$cat"/*.svg; do
		name="$(basename "$svg" .svg)"
		if node tools/svg2ivg/svg2ivg.js "$svg" | tail -n +2 > "$OUT_DIR/$cat/$name.ivg"; then
			output/IVG2PNG "$OUT_DIR/$cat/$name.ivg" "$OUT_DIR/$cat/$name.png" || true
		else
			echo "Failed to convert $svg"
		fi
	done
done

