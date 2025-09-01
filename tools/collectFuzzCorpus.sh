#!/usr/bin/env bash
set -e -o pipefail -u

cd "$(dirname "$0")"/..

if [[ $# -ne 1 ]]; then
	echo "Usage: bash tools/collectFuzzCorpus.sh <target-dir>" >&2
	exit 1
fi

target="$1"
mkdir -p "$target"
abs_target="$(cd "$target" && pwd)"
repo_root="$(pwd)"

find . -name '*.ivg' -print0 | while IFS= read -r -d '' file; do
	abs_file="$repo_root/${file#./}"
	case "$abs_file" in
		"$abs_target"/*) continue ;;
	esac
	dest="$abs_target/${file#./}"
	mkdir -p "$(dirname "$dest")"
	cp "$abs_file" "$dest"
done
