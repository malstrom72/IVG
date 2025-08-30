# Implementation Plan: Space-separated coordinate pairs

## Milestone 1: Parsing and instruction handlers
- [ ] Refactor `makeLinePath` to treat each argument as a single `x,y` pair and remove the `minimumCount` parameter.
- [ ] Update the `LINE` and `POLYGON` cases in `execute` to rely on argument count and call the new `makeLinePath` signature.
- [ ] Run `timeout 600 ./build.sh` and verify all tests pass.

## Milestone 2: Update tests
- [ ] Rewrite `tests/ivg/linePolygonTest.ivg` to use space-separated coordinate pairs for `LINE` and `POLYGON`.
- [ ] Run `timeout 600 ./build.sh` and verify all tests pass.

## Milestone 3: Documentation
- [ ] Update `docs/IVG Documentation.md`, `docs/IVG Documentation.html`, and `docs/IVG3 Roadmap.md` to show space-separated coordinate pairs.
- [ ] Run `timeout 600 ./build.sh` and verify all tests pass.

## Milestone 4: Finalization
- [ ] Commit the changes with message "Use space-separated coordinate pairs for LINE and POLYGON".
- [ ] Create a pull request summarizing the changes and referencing updated tests and docs.
