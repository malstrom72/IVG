# svg2ivg IVG-3 Upgrade Plan

This document outlines tasks for migrating `tools/svg2ivg.js` to fully leverage the IVG-3 format and generate more accurate output.

## Observations

- The `ellipse` converter emits comma-separated coordinates (`ellipse cx,cy,rx,ry`), which is the IVG-1/2 form.
- `<line>`, `<polyline>`, and `<polygon>` elements are serialized as `path svg:[...]` strings instead of using dedicated `LINE` and `POLYGON` instructions — fixing this will avoid unnecessary SVG parsing.
- `buildGradient` outputs comma-separated gradient coordinates (e.g., `linear x1,y1,x2,y2` and `radial cx,cy,r`). IVG-3 requires space-separated coordinate pairs, and the existing regression tests still use the older comma style.
- `stroke-dasharray` serialization joins values with commas; IVG-3 favors space-separated dash lengths.
- `buildPattern` and similar emitters still use comma-separated pairs for `bounds` and `offset`.
- Simple `<path>` elements composed solely of straight segments are not optimized; they remain as `path svg:` data.
- IVG follows a casing convention: drawing instructions like `RECT` and `LINE` are uppercase, while directives such as `fill` and
  `pen`—including segment commands inside `PATH` compounds—are lowercase.

## Goals

1. Use IVG-3 geometry instructions wherever possible.
2. Emit IVG-3 gradient syntax.
3. Adopt IVG-3 formatting for dash arrays and pattern coordinates.
4. Update tests and documentation to reflect the new behavior.
5. Follow the IVG casing convention so uppercase instructions draw and lowercase ones configure state.

## Checklist

- [x] Enforce the SVG tests run by `build.sh`/`tools/testSVG.sh` to always require PNG golden files and ensure all pass.

### Geometry instructions

- [x] Output `ELLIPSE cx,cy rx,ry` (or `ELLIPSE cx,cy r` for circles) instead of the comma-separated form.
- [x] Map `<line>` and `<polyline>` to `LINE x0,y0 x1,y1 ...`.
- [x] Map `<polygon>` to `POLYGON x0,y0 x1,y1 ...`.
- [x] Ensure marker processing and bounding boxes still work with the new instructions.
- [x] Collapse two-point `<polyline>` elements directly to `LINE`.

### Gradient syntax

- [x] Modify `buildGradient` to emit `linear x1,y1 x2,y2` and `radial cx,cy r`.
- [ ] Keep relative/transform options unchanged.

### Stroke and pattern syntax

- [x] Output dash arrays as space-separated values and preserve `stroke-dashoffset`.
- [ ] Emit pattern `bounds` and `offset` with space-separated coordinates if required.

### Instruction casing

- [x] Ensure `svg2ivg.js` emits drawing instructions in uppercase and directives/path segment commands in lowercase.

### Testing and docs

- [x] Update `tests/svg/supported` cases to expect `LINE`/`POLYGON` and the spaced ellipse/gradient syntax.
- [x] Add regression tests ensuring `LINE`/`POLYGON` appear instead of `path svg:` where dedicated IVG-3 instructions exist.
- [x] Document the IVG-3 requirement in the README and relevant guides.

### Future optimizations

- [ ] Detect simple `<path>` elements composed of straight segments and output `LINE`/`POLYLINE` instead of `path svg:`.
- [ ] Investigate translating SVG arc commands into an IVG `ARC` instruction.

## Expected Benefits

- Cleaner IVG files with explicit geometry and gradients.
- Less runtime parsing for ImpD.
- Easier future extensions and maintenance.
