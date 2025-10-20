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

cp "${output_dir}/rasterizeIVG.js" "${ext_dir}/media/rasterizeIVG.js"
cp "${output_dir}/setupModule.js" "${ext_dir}/media/setupModule.js"

docs_source="${output_dir}/docs"
docs_target="${ext_dir}/media/docs"
if [[ -d "${docs_source}" ]]; then
	mkdir -p "${docs_target}"
	if command -v rsync >/dev/null 2>&1; then
		rsync -a --delete "${docs_source}/" "${docs_target}/"
	else
		rm -rf "${docs_target}"
		mkdir -p "${docs_target}"
		( cd "${docs_source}" && tar cf - . ) | ( cd "${docs_target}" && tar xf - )
	fi
fi

echo "Updated IVGFiddle artifacts in ${ext_dir}/media" >&2
