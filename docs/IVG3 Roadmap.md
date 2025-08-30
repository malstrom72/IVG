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
LINE 10,10 80,40 40,80
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
POLYGON 20,20 120,20 120,80 20,80
```

- [ ] **PATH**

The `PATH` instruction draws arbitrary vector paths consisting of straight lines, Bezier curves, and arcs. It is filled with the current `fill` and outlined with the current `pen`.

**Syntax**

```
PATH svg:<svg data> | [ <instructions> ] [ closed:(yes|no)=no ]
```

**Instruction-list example**

```
fill lime
pen black
PATH [move-to 20,20; line-to 120,20 120,80 20,80] closed:yes
```

**Instructions**

```
move-to	  <x>,<y>
line-to	  <x>,<y> [<x>,<y> ...]
bezier-to <x>,<y> via:[<cx>,<cy>]
bezier-to <x>,<y> via:[<c1x>,<c1y>,<c2x>,<c2y>]
arc-to	  <x>,<y>,(<r>|<rx>,<ry>) [sweep:cw|ccw=cw] [large:yes|no=no] [rotate:<deg>=0]
arc-sweep <cx>,<cy>,<degrees>
```

- `move-to <x>,<y>` sets the current drawing point (must be the first instruction).
- `line-to <x>,<y> [<x>,<y> ...]` draws straight lines from the current point to one or more endpoints.
- `bezier-to <x>,<y> via:[cx,cy]` draws a quadratic Bezier curve.
- `bezier-to <x>,<y> via:[c1x,c1y,c2x,c2y]` draws a cubic Bezier curve using two control points.
- `arc-to <x>,<y>,(r|rx,ry)` draws an elliptical arc from the current point to `<x>,<y>`.
- `r` specifies a circular radius; `rx,ry` specify ellipse radii.
- `[sweep:cw|ccw=cw]` chooses direction.
- `[large:yes|no=no]` chooses larger or smaller arc.
- `[rotate:<deg>=0]` rotates the ellipse axes before tracing.
- `arc-sweep <cx>,<cy>,<degrees>` draws a circular arc around center `<cx>,<cy>`. Radius is the distance from the current point. Positive degrees sweep clockwise, negative sweep counterclockwise.
- `closed:(yes|no)=no` closes the path automatically, connecting the final point back to the first for both fill and stroke.

**Arc sweep example**

```
pen black
PATH [move-to 10,50; arc-sweep 50,50,180]
```



# Implementation plan

## Milestone 1 – helper stub and declaration
- [ ] In `src/IVG.cpp`, add `buildPathFromInstructions` after `buildPathFromSVG`. The stub should accept an `ImpDDecoder&` and `PathBuilder&` and return `bool`.
- [ ] Declare `buildPathFromInstructions` in `src/IVG.h` alongside the `buildPathFromSVG` prototype.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 2 – executor scaffold and dispatcher
- [ ] Introduce `PathInstructionExecutor` in `src/IVG.cpp` near existing instruction executors. Derive from `Executor` and forward `trace`, `progress`, and `load` methods to the passed `Decoder`.
- [ ] Run `node externals/QuickHashGen/QuickHashGenCLI.node.js --seed 1 <<<'move-to\nline-to\nbezier-to\narc-to\narc-sweep\n'` and paste the output into `src/IVG.cpp` as `findPathInstructionType`.
- [ ] Place `findPathInstructionType` with other `/* Built with QuickHashGen */` helpers and invoke it from `PathInstructionExecutor::execute` to dispatch based on the instruction name.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 3 – basic instructions
- [ ] Within `buildPathFromInstructions`, implement parsing for `move-to` (must be the first instruction) and `line-to`. Use `decoder.getFloat()` to read coordinate pairs until the argument list ends.
- [ ] Add support for `bezier-to`; read control points from the `via:` label and dispatch to quadratic or cubic `PathBuilder` methods accordingly.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 4 – arc handling
- [ ] Extract the arc maths used in the `'A'` case of `buildPathFromSVG` (around `src/IVG.cpp` lines 272–330) into a shared helper, e.g. `appendArcSegment`.
- [ ] Use `appendArcSegment` for `arc-to` within `buildPathFromInstructions` and replace the existing code in `buildPathFromSVG` to call the helper.
- [ ] Implement `arc-sweep` by computing the radius from the current point to the supplied center and delegating to `appendArcSegment`.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 5 – finalize path and integrate
- [ ] Respect `closed:yes` by calling `builder.closePath()` before returning from `buildPathFromInstructions`.
- [ ] In `IVGExecutor::execute`, modify the `PATH` case so that when no `svg:` argument is provided it calls `buildPathFromInstructions`; retain existing `svg:` support.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 6 – documentation
- [ ] In `docs/IVG Documentation.md`, expand the PATH section to cover instruction lists, detailing each sub-command with examples mirroring the ones above.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 7 – regression test
- [ ] Add `tests/ivg/pathInstructions.ivg` showing `move-to`, `line-to`, `bezier-to`, `arc-to`, `arc-sweep`, and `closed:yes` cases.
- [ ] Validate rendering with `bash tools/testIVG.sh tests/ivg/pathInstructions.ivg` and avoid committing generated `.png` files.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Final test
- [ ] `timeout 600 ./tools/buildAndTest.sh beta native nosimd`
