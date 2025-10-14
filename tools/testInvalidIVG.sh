#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
EXE=${1:-../output/InvalidIVGTest}

FILES=()
while IFS=$'\t' read -r _ path; do
	FILES+=("$path")
done < <(
	find ./ivg/invalid -type f -name '*.ivg' -print0 |
	while IFS= read -r -d '' path; do
		base=${path##*/}
		key=$(printf '%s' "$base" | tr '[:upper:]' '[:lower:]' | tr '_' ' ')
		printf '%s\t%s\n' "$key" "$path"
done |
	LC_ALL=C sort -t $'\t' -k1,1
)

if [ "$UPDATE" -eq 1 ]; then
	# Pass 1: update individual .err files when messages changed
	set +e
	for f in "${FILES[@]}"; do
		out=$("$EXE" "$f" 2>&1)
		# If a test failed, extract the actual message and refresh the .err file
		if printf '%s' "$out" | grep -q 'FAIL (got "'; then
			msg=$(printf '%s' "$out" | sed -n 's/^.*FAIL (got "\(.*\)").*$/\1/p')
			errFile="${f%.ivg}.err"
			printf '%s\n' "$msg" > "$errFile"
		fi
		# Do not stop on failures during update pass
	done
	set -e
	# Pass 2: regenerate the summary file with all tests (should now be PASS)
	for f in "${FILES[@]}"; do
		"$EXE" "$f"
	done | tee invalidIVGResults.txt
else
	# Run all tests, do not stop early, but exit non-zero if any failed.
	set +e
	fail=0
	for f in "${FILES[@]}"; do
		"$EXE" "$f"
		[ "$?" -ne 0 ] && fail=1
	done
	set -e
	exit $fail
fi
