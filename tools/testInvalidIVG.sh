#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
EXE=${1:-../output/InvalidIVGTest}

if [ "$UPDATE" -eq 1 ]; then
	# Pass 1: update individual .err files when messages changed
	set +e
	for f in ./ivg/invalid/*.ivg; do
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
	for f in ./ivg/invalid/*.ivg; do
		"$EXE" "$f"
	done | tee invalidIVGResults.txt
else
	for f in ./ivg/invalid/*.ivg; do
		"$EXE" "$f"
	done
fi
