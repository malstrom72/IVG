#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/..

CPP_OPTIONS="-std=c++03 -fsanitize=fuzzer,address" \
	bash ./tools/PikaCmd/SourceDistribution/BuildCpp.sh beta native output/PikaCmdFuzz \
	-DLIBFUZZ -DPLATFORM_STRING=UNIX -I src \
	tools/PikaCmd/PikaCmd.cpp tools/PikaCmd/BuiltIns.cpp src/PikaScript.cpp src/QStrings.cpp
