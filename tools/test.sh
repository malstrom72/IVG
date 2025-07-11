#!/bin/bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests

echo "### GOOD TESTS ###"
echo
build/Debug/IMPDTest <goodTests.impd | diff --strip-trailing-cr goodResults.txt -
echo
echo "### BAD TESTS ###"
echo
build/Debug/IMPDTest <badTests.impd | diff --strip-trailing-cr badResults.txt -
