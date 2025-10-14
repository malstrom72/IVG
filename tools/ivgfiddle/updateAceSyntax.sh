#!/usr/bin/env bash
set -e -o pipefail -u
cd "$(dirname "$0")"/../..

# Regenerate Ace highlight rules from .tmLanguage and build a bundled mode (mode-ivg.js).
# This script is IVGFiddle-specific and always runs dryice to produce a self-contained mode.

ACE_DIR=externals/ace
ACE_REF=${ACE_REF:-v1.43.3}
DEST_DIR=tools/ivgfiddle/src/ace
TM_IVG=tools/grammars/ivg.tmLanguage
TM_IMPD=tools/grammars/impd.tmLanguage

log() {
	printf '%s\n' "$*"
}

need() {
	if ! command -v "$1" >/dev/null 2>&1; then
		log "Missing required tool: $1"
		exit 1
	fi
}

need git
need node
need npm

if [ ! -d "$ACE_DIR/.git" ]; then
	log "Cloning Ace toolchain into $ACE_DIR ..."
	git clone --depth 1 https://github.com/ajaxorg/ace.git "$ACE_DIR"
else
	log "Ace already present at $ACE_DIR (skipping clone)."
fi

(
	cd "$ACE_DIR"
	git fetch --tags --depth 1 >/dev/null 2>&1 || true
	if git checkout -q "$ACE_REF" 2>/dev/null || git checkout -q "tags/$ACE_REF" 2>/dev/null; then
		log "Using Ace converter at ref: $ACE_REF"
	else
		log "Warning: Could not checkout '$ACE_REF'; using current HEAD of Ace."
	fi
	log "Installing converter deps (amd-loader, plist, cson) ..."
	npm i --no-audit --no-fund amd-loader plist cson >/dev/null 2>&1 || true
)

# Verify converter deps from Ace dir
if ! (
	cd "$ACE_DIR" && node -e 'require("amd-loader");require("plist");require("cson");' >/dev/null 2>&1
); then
	log "Converter dependencies are missing or could not be loaded."
	log "Re-run when online to regenerate syntax."
	exit 1
fi

log "Converting tmLanguage → Ace rules (ivg/impd) ..."
node "$ACE_DIR/tool/tmlanguage.js" "$TM_IMPD" "$TM_IVG"

# Build the bundled mode with dryice so IVGFiddle can load a single file
log "Building Ace bundle with dryice (-m -nc) ..."
(
	cd "$ACE_DIR"
	node Makefile.dryice.js -m -nc >/dev/null
)

BUNDLED_SRC="$ACE_DIR/build/src-min-noconflict/mode-ivg.js"
if [ ! -f "$BUNDLED_SRC" ]; then
	log "Error: dryice bundle not found at $BUNDLED_SRC"
	exit 1
fi

mkdir -p "$DEST_DIR"
cp "$BUNDLED_SRC" "$DEST_DIR/mode-ivg.js"
log "Bundled mode updated: $DEST_DIR/mode-ivg.js"

log "Note: Normal builds do not require this step; this is only for refreshing IVGFiddle syntax."
