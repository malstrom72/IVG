# Path instruction support plan

We plan to add support for the following instructions in path definitions without requiring an initial `move-to`: `line`, `rect`, `ellipse`, `star`, `polygon`, and `text`.

The guiding goal is minimal source code: factor shared logic and keep the number of statements as low as possible.

Before editing files, convert tabs to spaces with `expand -t 4 file | sponge file` and restore tabs afterward with `unexpand -t 4 file | sponge file`.

## QuickHashGen lookup table
- [x] Maintain `findPathInstructionType` using [QuickHashGen](../externals/QuickHashGen/README.md) for constant-time string lookup.
- [x] Create a newline-delimited list of all path instruction names and run `node externals/QuickHashGen/QuickHashGenCLI.node.js --seed 1 < list.txt > tmp.c`.
- [x] Copy the generated `STRINGS` and `QUICK_HASH_TABLE` arrays into `src/IVG.cpp`'s `findPathInstructionType` and keep the `/* Built with QuickHashGen */` header.
- [x] Re-run QuickHashGen whenever instructions are added or removed so the table stays collision free.

## Path start tracking
- [x] Remove the `moveToSeen` flag from `PathInstructionExecutor`.
- [x] Have `checkHasMoveTo` inspect the `Path` directly (e.g. `path.empty()` or verifying the first instruction is `MOVE`).
- [x] Adjust callers so instructions that construct their own starting point (like `line` or `rect`) work without extra bookkeeping.

## Milestone 1 – Refactor shape builders
- [x] Extract reusable path-building helpers from drawing instructions in `src/IVG.cpp`:
- [x] `makeLinePath` for poly-lines.
- [x] Rectangle, ellipse, star, and polygon builders that currently construct temporary `Path` objects.
- [x] `buildPathForString` for converting text to glyph outlines.
- [x] Move these helpers to a dedicated section (or header) so both drawing and path execution share the same code.
- [x] Update drawing instruction handlers (`LINE_INSTRUCTION`, `RECT_INSTRUCTION`, `ELLIPSE_INSTRUCTION`, `STAR_INSTRUCTION`, `POLYGON_INSTRUCTION`, and text drawing) to call the helpers.
- [x] Run `timeout 600 ./build.sh` and ensure all tests pass.

## Milestone 2 – Add `line` and `rect` path instructions
- [x] Extend `enum PathInstructionType` in `src/IVG.cpp` with `LINE_INSTRUCTION` and `RECT_INSTRUCTION` values.
- [x] Add `"line"` and `"rect"` to the QuickHashGen list and regenerate `findPathInstructionType`.
- [x] In `PathInstructionExecutor::execute`:
	   - [x] `LINE_INSTRUCTION`: parse point pairs and append the `makeLinePath` result; the helper issues its own `moveTo` so a prior `move-to` isn't required.
	   - [x] `RECT_INSTRUCTION`: parse `x y w h` plus optional `rounded`; call rectangle helper to append to the current path.
- [x] Add regression tests demonstrating `line` and `rect` within path definitions.
- [x] Run `timeout 600 ./build.sh`.

## Milestone 3 – Add `ellipse`, `star`, and `polygon` path instructions
- [x] Extend `enum PathInstructionType` with `ELLIPSE_INSTRUCTION`, `STAR_INSTRUCTION`, and `POLYGON_INSTRUCTION`.
- [x] Add their names to the QuickHashGen list and regenerate `findPathInstructionType`.
- [x] In `PathInstructionExecutor::execute`:
	   - [x] `ELLIPSE_INSTRUCTION`: parse center and radii, call ellipse helper (`Path::addEllipse`/`addCircle`).
	   - [x] `STAR_INSTRUCTION`: parse center, point count, radii, and optional rotation; use `Path::addStar`.
	   - [x] `POLYGON_INSTRUCTION`: reuse `makeLinePath` for vertices then call `path.close()`.
- [x] Add regression tests for `ellipse`, `star`, and `polygon` path instructions.
- [x] Run `timeout 600 ./build.sh`.

## Milestone 4 – Add `text` path instruction
- [x] Extend parser and `enum PathInstructionType` with `TEXT_INSTRUCTION`.
- [x] Include `"text"` in the QuickHashGen list and regenerate `findPathInstructionType`.
- [x] In `PathInstructionExecutor::execute`, reuse `buildPathForString` to convert glyphs to a path at the current location; ensure font selection and anchor handling mirror drawing primitives.
- [x] Add regression tests covering `text` in path definitions.
- [x] Run `timeout 600 ./build.sh`.
