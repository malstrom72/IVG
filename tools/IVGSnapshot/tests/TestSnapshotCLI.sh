#!/usr/bin/env bash
set -e -o pipefail -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
cd "$repo_root"

snapshot_tool="./output/IVGSnapshot"
if [ ! -x "$snapshot_tool" ]; then
	printf 'Snapshot tool not built: %s\n' "$snapshot_tool" >&2
	exit 1
fi

sample_ivg="tools/IVGSnapshot/tests/ListOnlySample.ivg"
sample_txt="tools/IVGSnapshot/tests/ListOnlySample.txt"

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

"$snapshot_tool" --list-only "$sample_ivg" >"$tmp"
diff --strip-trailing-cr "$sample_txt" "$tmp"

set +e
missing_output=$("$snapshot_tool" --include-dir 2>&1)
missing_status=$?
set -e
if [ $missing_status -eq 0 ]; then
	echo "expected --include-dir without value to fail" >&2
	echo "$missing_output" >&2
	exit 1
fi
if ! printf '%s' "$missing_output" | grep -F -q -- "--include-dir requires a path."; then
	echo "missing argument error message mismatch" >&2
	echo "$missing_output" >&2
	exit 1
fi

set +e
unknown_output=$("$snapshot_tool" --bogus "$sample_ivg" 2>&1)
unknown_status=$?
set -e
if [ $unknown_status -eq 0 ]; then
	echo "expected unknown option to fail" >&2
	echo "$unknown_output" >&2
	exit 1
fi
if ! printf '%s' "$unknown_output" | grep -F -q -- "unrecognized option: --bogus"; then
	echo "unknown option message mismatch" >&2
	echo "$unknown_output" >&2
	exit 1
fi

set +e
threads_output=$("$snapshot_tool" --threads nope "$sample_ivg" 2>&1)
threads_status=$?
set -e
if [ $threads_status -eq 0 ]; then
	echo "expected invalid thread count to fail" >&2
	echo "$threads_output" >&2
	exit 1
fi
if ! printf '%s' "$threads_output" | grep -F -q -- "invalid thread count: nope"; then
	echo "threads error message mismatch" >&2
	echo "$threads_output" >&2
	exit 1
fi

echo "CLI parsing tests passed"
