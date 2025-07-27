#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"
which -s pika || curl -fsSL https://nuedge.net/pikascript/install.sh | sh
which -s pandoc || brew install pandoc
pika updateDocumentationImages.pika "../docs/IVG Documentation.md"
pandoc -s -o "../docs/ImpD Documentation.html" --metadata title="ImpD Documentation" --include-in-header pandoc.css "../docs/ImpD Documentation.md"
pandoc -s -o "../docs/IVG Documentation.html" --metadata title="IVG Documentation" --include-in-header pandoc.css "../docs/IVG Documentation.md"

# Generate PDF documentation using Doxygen if available
if command -v doxygen >/dev/null; then
	doxygen ../docs/Doxyfile
	if [ -f ../docs/doxygen/latex/Makefile ]; then
		make -s -C ../docs/doxygen/latex pdf
		cp ../docs/doxygen/latex/refman.pdf "../docs/IVG Documentation.pdf"
	fi
	else
	echo "Warning: doxygen not found. Skipping PDF documentation update" >&2
	fi
