# Invalid IVG Test Plan

This document describes how to create a regression suite that ensures malformed IVG files emit the expected diagnostics.

## Goals
- Confirm that malformed input fails during parsing.
- Verify that each failure reports a predictable error message or code.

## Test Harness
1. Add a small C++ test program `tests/invalid_ivg.cpp`.
	- Program iterates over files in `tests/ivg/invalid/`.
	- Each file is loaded through the existing IVG loader.
	- The loader must throw `SyntaxException` or `FormatException`.
2. Implement helper to compare the thrown message with a reference `.err` file.

## Test Data
- Store malformed samples under `tests/ivg/invalid/`.
- Keep each sample minimal and annotate the defect in a comment.
- Provide a companion `.err` file with the expected diagnostic for every sample.

## Test Batches
1. **Format Directive**
	- missing `format` line
	- `format IVG-4` reports unsupported version
	- `requires:` listing an unknown feature
2. **Paint and Color**
	- `fill` with invalid hex color or pre-multiplied value
	- gradient with unrecognized type
	- IVG-3 file using comma-separated gradient coordinates
	- gradient stops with an odd element count or out-of-range position
3. **Geometry Instructions**
	- `PATH` arc-to with invalid `sweep`/`large` flags
	- `ELLIPSE` or `STAR` syntax using the wrong separator for the IVG version
	- unknown command letter inside a `PATH` instruction
4. **Stroke and Fill**
	- unrecognized stroke caps or joints
	- unrecognized fill rule
5. **Text and Alignment**
	- invalid or duplicate alignment tokens in a `text` instruction
	- `text` referencing an undefined font
6. **Font Definitions**
	- duplicate `metrics` instruction in a `font` block
	- `glyph` appearing before `metrics`
	- glyph name longer than one character
	- duplicate glyph or kerning pair
7. **Definitions and References**
	- `define` with an unsupported type
	- `fill pattern:id` referencing an undefined pattern or mask
	- `context` block missing required `bounds` when drawing an image

## Integration
- Extend `build.sh` to invoke the new program as part of the test run.
- Record expected messages in a results file similar to `tests/badResults.txt`.
- Document the workflow for adding new invalid cases.

## Future Work
- Add runtime validation cases that require execution beyond parsing.
- Generate fuzzed inputs to broaden coverage.
