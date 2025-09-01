#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"
bash "../externals/PikaCmd/BuildPikaCmd.sh"
if ! command -v pandoc >/dev/null 2>&1; then
	echo "pandoc not found. Install it first." >&2
	exit 1
fi
bash "../externals/PikaCmd/pika" updateDocumentationImages.pika "../docs/IVG Documentation.md"
pandoc -s -o "../docs/ImpD Documentation.html" --metadata title="ImpD Documentation" --include-in-header pandoc.css "../docs/ImpD Documentation.md"
pandoc -s -o "../docs/IVG Documentation.html" --metadata title="IVG Documentation" --include-in-header pandoc.css "../docs/IVG Documentation.md"
