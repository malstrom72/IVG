#!/usr/bin/env bash
set -e -o pipefail -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
cd "$repo_root"

snapshot_tool="./output/IVGSnapshot"
if [ ! -x "$snapshot_tool" ]; then
	echo "Snapshot tool not built: $snapshot_tool" >&2
	exit 1
fi

temp_dir="$(mktemp -d)"
trap 'rm -rf "$temp_dir"' EXIT

ivg_file="$temp_dir/snaptest.ivg"
cat <<'SNAP' > "$ivg_file"
format ivg-3 uses:snapshot-1
bounds 0,0,37,37
meta snapshot validate:no [
	color=#E74C3C
	highlight=#FFFFFF
	shadow=#000000
]
FILL $color
ELLIPSE 15,15,14
SNAP

output_dir="$temp_dir/snapshots"
mkdir -p "$output_dir"

run_draft="$("$snapshot_tool" --snapshot-dir "$output_dir" "$ivg_file")"
printf '%s\n' "$run_draft"

snapshot_prefix="${ivg_file%.*}"
snapshot_prefix="${snapshot_prefix//\\/_}"
snapshot_prefix="${snapshot_prefix//\//_}"
snapshot_prefix="${snapshot_prefix//:/_}"

golden_path="$output_dir/${snapshot_prefix}__snaptest-1.png"
old_path="$output_dir/${snapshot_prefix}__snaptest-1.png.old"
if [ ! -f "$old_path" ] && [ ! -f "$golden_path" ]; then
	echo "Draft run did not produce a .png.old artifact." >&2
	exit 1
fi

cat <<'SNAP' > "$ivg_file"
format ivg-3 uses:snapshot-1
bounds 0,0,37,37
meta snapshot validate:yes [
	color=#E74C3C
	highlight=#FFFFFF
	shadow=#000000
]
FILL $color
ELLIPSE 15,15,14
SNAP

run_validate1="$("$snapshot_tool" --snapshot-dir "$output_dir" "$ivg_file")"
printf '%s\n' "$run_validate1"

if printf '%s\n' "$run_validate1" | grep -q "FAILED"; then
	echo "Initial validation failed." >&2
	exit 1
fi

run_validate2="$("$snapshot_tool" --snapshot-dir "$output_dir" "$ivg_file")"
printf '%s\n' "$run_validate2"

if printf '%s\n' "$run_validate2" | grep -q "FAILED"; then
	echo "Second validation run reported a failure." >&2
	exit 1
fi

echo "Workflow validation completed without diffs."
