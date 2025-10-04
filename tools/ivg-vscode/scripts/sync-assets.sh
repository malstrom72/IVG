#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
ext_dir="${repo_root}/tools/ivg-vscode"
output_dir="${repo_root}/tools/ivgfiddle/output"

build=0
if [[ "${1-}" == "--build" ]]; then
	build=1
	shift
fi

if [[ $# -gt 0 ]]; then
	echo "Usage: $(basename "$0") [--build]" >&2
	exit 1
fi

cd "${repo_root}"

if [[ ${build} -eq 1 ]]; then
	bash tools/ivgfiddle/buildIVGFiddle.sh
fi

if [[ ! -f "${output_dir}/ivgfiddle.html" ]]; then
	echo "Expected ${output_dir}/ivgfiddle.html but it was not found." >&2
	exit 1
fi

mkdir -p "${ext_dir}/media"

if command -v rsync >/dev/null 2>&1; then
	rsync -a --delete "${output_dir}/" "${ext_dir}/media/"
else
	rm -rf "${ext_dir}/media"
	mkdir -p "${ext_dir}/media"
	( cd "${output_dir}" && tar cf - . ) | ( cd "${ext_dir}/media" && tar xf - )
fi

echo "Synchronized assets into ${ext_dir}/media" >&2
