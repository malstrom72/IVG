# Path instruction support plan

We plan to add support for the following instructions in path definitions without requiring an initial `move-to`: `line`, `rect`, `ellipse`, `star`, `polygon`, and `text`.

## QuickHashGen lookup table
- [ ] Maintain `findPathInstructionType` using [QuickHashGen](../externals/QuickHashGen/README.md) for constant-time string lookup.
- [ ] Create a newline-delimited list of all path instruction names and run `node externals/QuickHashGen/QuickHashGenCLI.node.js --seed 1 < list.txt > tmp.c`.
- [ ] Copy the generated `STRINGS` and `QUICK_HASH_TABLE` arrays into `src/IVG.cpp`'s `findPathInstructionType` and keep the `/* Built with QuickHashGen */` header.
- [ ] Re-run QuickHashGen whenever instructions are added or removed so the table stays collision free.

## Milestone 1 – Refactor shape builders
- [ ] Extract reusable path-building helpers from drawing instructions in `src/IVG.cpp`:
	- [ ] `makeLinePath` for poly-lines.
	- [ ] Rectangle, ellipse, star, and polygon builders that currently construct temporary `Path` objects.
	- [ ] `buildPathForString` for converting text to glyph outlines.
- [ ] Move these helpers to a dedicated section (or header) so both drawing and path execution share the same code.
- [ ] Update drawing instruction handlers (`LINE_INSTRUCTION`, `RECT_INSTRUCTION`, `ELLIPSE_INSTRUCTION`, `STAR_INSTRUCTION`, `POLYGON_INSTRUCTION`, and text drawing) to call the helpers.
- [ ] Run `timeout 600 ./build.sh` and ensure all tests pass.

## Milestone 2 – Add `line` and `rect` path instructions
- [ ] Extend `enum PathInstructionType` in `src/IVG.cpp` with `LINE_INSTRUCTION` and `RECT_INSTRUCTION` values.
- [ ] Add `"line"` and `"rect"` to the QuickHashGen list and regenerate `findPathInstructionType`.
- [ ] In `PathInstructionExecutor::execute`:
	- [ ] `LINE_INSTRUCTION`: parse point pairs and append the `makeLinePath` result, setting `moveToSeen = true` automatically.
	- [ ] `RECT_INSTRUCTION`: parse `x y w h` plus optional `rounded`; call rectangle helper to append to the current path.
- [ ] Add regression tests demonstrating `line` and `rect` within path definitions.
- [ ] Run `timeout 600 ./build.sh`.

## Milestone 3 – Add `ellipse`, `star`, and `polygon` path instructions
- [ ] Extend `enum PathInstructionType` with `ELLIPSE_INSTRUCTION`, `STAR_INSTRUCTION`, and `POLYGON_INSTRUCTION`.
- [ ] Add their names to the QuickHashGen list and regenerate `findPathInstructionType`.
- [ ] In `PathInstructionExecutor::execute`:
	- [ ] `ELLIPSE_INSTRUCTION`: parse center and radii, call ellipse helper (`Path::addEllipse`/`addCircle`).
	- [ ] `STAR_INSTRUCTION`: parse center, point count, radii, and optional rotation; use `Path::addStar`.
	- [ ] `POLYGON_INSTRUCTION`: reuse `makeLinePath` for vertices then call `path.close()`.
- [ ] Add regression tests for `ellipse`, `star`, and `polygon` path instructions.
- [ ] Run `timeout 600 ./build.sh`.

## Milestone 4 – Add `text` path instruction
- [ ] Extend parser and `enum PathInstructionType` with `TEXT_INSTRUCTION`.
- [ ] Include `"text"` in the QuickHashGen list and regenerate `findPathInstructionType`.
- [ ] In `PathInstructionExecutor::execute`, reuse `buildPathForString` to convert glyphs to a path at the current location; ensure font selection and anchor handling mirror drawing primitives.
- [ ] Add regression tests covering `text` in path definitions.
- [ ] Run `timeout 600 ./build.sh`.
