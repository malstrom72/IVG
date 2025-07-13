#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../tests
../output/IMPDTest <goodTests.impd >goodResults.txt
../output/IMPDTest <badTests.impd >badResults.txt
