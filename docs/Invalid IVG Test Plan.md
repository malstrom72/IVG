# Invalid IVG Test Plan

This document describes how to create a regression suite that ensures malformed IVG files emit the expected diagnostics.

## Goals
- [ ] Confirm that malformed input fails during parsing.
- [ ] Verify that each failure reports a predictable error message or code.

## Test Harness
1. [x] Add a small C++ test program `tests/invalid_ivg.cpp`.
	- [x] Program iterates over files in `tests/ivg/invalid/`.
	- [x] Each file is loaded through the existing IVG loader.
	- [x] The loader must throw `SyntaxException` or `FormatException`.
2. [x] Implement helper to compare the thrown message with a reference `.err` file.

## Test Data
- [x] Store malformed samples under `tests/ivg/invalid/`.
- [x] Keep each sample minimal and annotate the defect in a comment.
- [x] Provide a companion `.err` file with the expected diagnostic for every sample.

## Test Batches
1. **Format Directive**
		- [x] missing `format` line
		- [x] `format IVG-4` reports unsupported version
		- [x] `requires:` listing an unknown feature
2. **Paint and Color**
		- [x] `fill` with invalid hex color or pre-multiplied value
		- [x] `fill` with invalid color name
		- [x] gradient with unrecognized type
		- [x] IVG-3 file using comma-separated gradient coordinates
		- [x] gradient stops with an odd element count or out-of-range position
3. **Geometry Instructions**
		- [x] `PATH` arc-to with invalid `sweep`/`large` flags
		- [x] `ELLIPSE` or `STAR` syntax using the wrong separator for the IVG version
		- [x] unknown command letter inside a `PATH` instruction
		- [x] `LINE` with missing coordinate pair
4. **Stroke and Fill**
		- [x] unrecognized stroke caps or joints
		- [x] unrecognized fill rule
5. **Text and Alignment**
	- [x] invalid `anchor` token in a `text` instruction
	- [x] invalid alignment token in an `image` instruction
	- [x] `text` referencing an undefined font
6. **Font Definitions**
	- [x] duplicate `metrics` instruction in a `font` block
	- [x] `glyph` appearing before `metrics`
	- [x] glyph name longer than one character
	- [x] duplicate glyph or kerning pair
7. **Definitions and References**
	- [x] `define` with an unsupported type
	- [ ] `fill pattern:id` referencing an undefined pattern or mask
	- [x] `pattern` block missing required `bounds`

## Integration
- [x] Extend `build.sh` to invoke the new program as part of the test run.
- [ ] Record expected messages in a results file similar to `tests/badResults.txt`.
- [ ] Document the workflow for adding new invalid cases.

## Future Work
- [ ] Add runtime validation cases that require execution beyond parsing.
- [ ] Generate fuzzed inputs to broaden coverage.
