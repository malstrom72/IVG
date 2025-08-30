# IVG3 Roadmap

Tracking support for IVG3 drawing instructions.

## Instructions

- [x] **LINE**

The `LINE` instruction draws an open polyline using the current `pen`.

**Syntax**

```
LINE <x0>,<y0>[,<x1>,<y1> ...]
```

**Example**

```
pen black width:2
LINE 10,10, 80,40, 40,80
```

- [x] **POLYGON**

The `POLYGON` instruction draws a closed polygon using the current `fill` and `pen`.

**Syntax**

```
POLYGON <x0>,<y0>[,<x1>,<y1> ...]
```

**Example**

```
fill lime
pen black
POLYGON 20,20, 120,20, 120,80, 20,80
```

- [ ] **PATH**

The `PATH` instruction draws arbitrary vector paths consisting of straight lines, Bezier curves, and arcs. It is filled with the current `fill` and outlined with the current `pen`.

**Syntax**

```
PATH svg:<svg data> | ( <instructions> [ closed:(yes|no)=no ] )
```

**Instructions**

```
move-to	  <x>,<y>
line-to	  <x>,<y>[,<x>,<y>...]
bezier-to <x>,<y> via:[<cx>,<cy>]
bezier-to <x>,<y> via:[<c1x>,<c1y>,<c2x>,<c2y>]
arc-to	  <x>,<y>,(<r>|<rx>,<ry>) [sweep:cw|ccw=cw] [large:yes|no=no] [rotate:<deg>=0]
arc-sweep <cx>,<cy>,<degrees>
```

- `move-to <x>,<y>` sets the current drawing point (must be the first instruction).
- `line-to <x>,<y>[,<x>,<y>...]` draws straight lines from the current point to one or more endpoints.
- `bezier-to <x>,<y> via:[cx,cy]` draws a quadratic Bezier curve.
- `bezier-to <x>,<y> via:[c1x,c1y,c2x,c2y]` draws a cubic Bezier curve using two control points.
- `arc-to <x>,<y>,(r|rx,ry)` draws an elliptical arc from the current point to `<x>,<y>`.
- `r` specifies a circular radius; `rx,ry` specify ellipse radii.
- `[sweep:cw|ccw=cw]` chooses direction.
- `[large:yes|no=no]` chooses larger or smaller arc.
- `[rotate:<deg>=0]` rotates the ellipse axes before tracing.
- `arc-sweep <cx>,<cy>,<degrees>` draws a circular arc around center `<cx>,<cy>`. Radius is the distance from the current point. Positive degrees sweep clockwise, negative sweep counterclockwise.
- `closed:(yes|no)=no` closes the path automatically, connecting the final point back to the first for both fill and stroke.



# Implementation plan

## Milestone 1 – helper stub and declaration
- [ ] Add `buildPathFromInstructions` next to `buildPathFromSVG` in `src/IVG.cpp`.
- [ ] Declare the helper in `src/IVG.h`.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 2 – executor scaffold and dispatcher
- [ ] Introduce `PathInstructionExecutor` deriving from `Executor` and forwarding `trace`, `progress`, and `load`.
- [ ] Generate QuickHash dispatcher with `node externals/QuickHashGen/QuickHashGenCLI.node.js --seed 1 <<<'move-to\nline-to\nbezier-to\narc-to\narc-sweep\n'`.
- [ ] Place `findPathInstructionType` near other QuickHashGen blocks and call it from `execute`.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 3 – basic instructions
- [ ] Parse `move-to` (must come first) and `line-to` (accept multiple pairs).
- [ ] Parse `bezier-to` using `via:` list for quadratic/cubic control points.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 4 – arc handling
- [ ] Implement `arc-to` reusing math from `'A'` case of `buildPathFromSVG`.
- [ ] Implement `arc-sweep` starting sweep from the current point.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 5 – finalize path and integrate
- [ ] Close path when `closed:yes`.
- [ ] In `IVGExecutor::execute` PATH branch, call the helper when no `svg:` argument is present and keep existing `svg:` support.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 6 – documentation
- [ ] In `docs/IVG Documentation.md`, expand the PATH section to cover instruction lists with examples.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 7 – regression test
- [ ] Add `tests/ivg/pathInstructions.ivg` demonstrating the new syntax.
- [ ] Test by generating PNGs on the fly; do not commit any `.png` files.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Final test
- [ ] `timeout 300 ./tools/buildAndTest.sh beta native nosimd`
