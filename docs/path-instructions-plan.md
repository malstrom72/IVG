# Path instruction support plan

We plan to add support for the following instructions in path definitions without requiring an initial `move-to`: `line`, `rect`, `ellipse`, `star`, `polygon`, and `text`.

## Milestone 1 – Refactor shape builders
- [ ] Extract reusable path-building helpers from existing drawing primitives in `src/IVG.cpp`.
- [ ] Update drawing instructions to call the new helpers.
- [ ] Run `timeout 600 ./build.sh` and ensure all tests pass.

## Milestone 2 – Add `line` and `rect` path instructions
- [ ] Extend `findPathInstructionType` and related enums to include `line` and `rect`.
- [ ] In `PathInstructionExecutor::execute`, use shape helpers to append lines and rectangles to the path without requiring `move-to`.
- [ ] Add regression tests demonstrating `line` and `rect` within path definitions.
- [ ] Run `timeout 600 ./build.sh`.

## Milestone 3 – Add `ellipse`, `star`, and `polygon` path instructions
- [ ] Include these identifiers in `findPathInstructionType` and enum.
- [ ] Reuse shape helpers to append ellipses, stars, and polygons without a starting `move-to`.
- [ ] Add regression tests for `ellipse`, `star`, and `polygon` path instructions.
- [ ] Run `timeout 600 ./build.sh`.

## Milestone 4 – Add `text` path instruction
- [ ] Extend parser and enum to recognize `text`.
- [ ] Reuse existing text-to-path conversion to append glyph paths; ensure positioning and font selection mirror drawing primitive behavior.
- [ ] Add regression tests covering `text` in path definitions.
- [ ] Run `timeout 600 ./build.sh`.
