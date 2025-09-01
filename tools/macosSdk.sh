#!/usr/bin/env bash
set -e -o pipefail -u

cd "$(dirname "$0")"/..

sys="$(uname -s)"
if [ "$sys" = "Darwin" ] && command -v xcrun >/dev/null 2>&1; then
	sdkroot="$(xcrun --sdk macosx --show-sdk-path 2>/dev/null || true)"
	if [ -n "$sdkroot" ] && [ -d "$sdkroot" ]; then
		export SDKROOT="$sdkroot"
		echo "-isysroot $sdkroot"
	fi
fi
