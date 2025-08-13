#!/usr/bin/env bash
set -e -o pipefail -u

cd "$(dirname "$0")"

find tests/svg -type f -name '*.svg' | while IFS= read -r svg; do
	base=$(basename "$svg")
	[[ $base == resvg_tests_* ]] || continue
	name=${base%.svg}
	rest=${name#resvg_tests_}
	rel=${rest//_/\/}
	src="externals/resvgTests/$rel.png"
	if [[ -f $src ]]; then
		cp "$src" "$(dirname "$svg")/$name.png"
	else
		echo "missing $src" >&2
	fi
done
