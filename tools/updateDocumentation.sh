#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"
if [ ! -x ../externals/PikaCmd/output/PikaCmd ]; then
	bash ../externals/PikaCmd/build.sh
fi
which -s pandoc || brew install pandoc
../externals/PikaCmd/output/PikaCmd updateDocumentationImages.pika "../docs/IVG Documentation.md"
pandoc -s -o "../docs/ImpD Documentation.html" --metadata title="ImpD Documentation" --include-in-header pandoc.css "../docs/ImpD Documentation.md"
pandoc -s -o "../docs/IVG Documentation.html" --metadata title="IVG Documentation" --include-in-header pandoc.css "../docs/IVG Documentation.md"
