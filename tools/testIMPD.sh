#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

UPDATE=0
if [ "${1-}" = "update" ]; then
	UPDATE=1
	shift
fi
if [ "$UPDATE" -eq 1 ]; then
	echo "### GOOD TESTS ###"
	echo
	build/Debug/IMPDTest <goodTests.impd | tee goodResults.txt
	echo
	echo "### BAD TESTS ###"
	echo
	build/Debug/IMPDTest <badTests.impd | tee badResults.txt
else
	echo "### GOOD TESTS ###"
	echo
	build/Debug/IMPDTest <goodTests.impd | diff --strip-trailing-cr goodResults.txt -
	echo
	echo "### BAD TESTS ###"
	echo
	build/Debug/IMPDTest <badTests.impd | diff --strip-trailing-cr badResults.txt -
fi
