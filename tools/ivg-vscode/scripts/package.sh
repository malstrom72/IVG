#!/usr/bin/env bash
set -e -o pipefail -u

cd "$(dirname "$0")"/..

bash scripts/sync-assets.sh --build

npm install --no-audit --no-fund

npm run compile

if command -v vsce >/dev/null 2>&1; then
	VSCE_BIN="vsce"
	VSCE_ARGS=(package)
else
	VSCE_BIN="npx"
	VSCE_ARGS=(vsce package)
fi

"$VSCE_BIN" "${VSCE_ARGS[@]}" "$@"
