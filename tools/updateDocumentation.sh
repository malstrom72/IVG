#!/bin/bash
set -e -o pipefail -u
cd "$(dirname "$0")"
which -s pika || curl -s http://nuedge.net/pikascript/install.sh | sh
which -s pandoc || brew install pandoc
pika updateDocumentationImages.pika "../docs/IVG Documentation.md"
pandoc -s -o "../docs/ImpD Documentation.html" --metadata title="ImpD Documentation" --include-in-header pandoc.css "../docs/ImpD Documentation.md"
pandoc -s -o "../docs/IVG Documentation.html" --metadata title="IVG Documentation" --include-in-header pandoc.css "../docs/IVG Documentation.md"
