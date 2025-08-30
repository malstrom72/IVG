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
- [ ] In `src/IVG.cpp`, add `buildPathFromInstructions` immediately after `buildPathFromSVG` (function ends around line 346). The stub should accept `IMPD::Interpreter& impd`, `const IMPD::String& instructionBlock`, `double curveQuality`, and `NuXPixels::Path& path`, then simply `return false;` with a `// TODO` comment so the body is easy to locate later.
- [ ] Declare `buildPathFromInstructions` in `src/IVG.h` directly beneath the `buildPathFromSVG` prototype using the same namespaces (`IMPD` and `NuXPixels`) as the existing declarations.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 2 – executor scaffold and dispatcher
- [ ] Introduce `PathInstructionExecutor` in `src/IVG.cpp` near the other executor helpers (around line 575 next to `TransformationExecutor`).
      * Derive from `IMPD::Executor` and store references to the parent executor, the `NuXPixels::Path` to populate, and the `double curveQuality` passed in through the constructor.
      * Forward the `trace`, `progress`, and `load` calls to the parent executor in the same way `TransformationExecutor` forwards to its parent.
- [ ] Run `node externals/QuickHashGen/QuickHashGenCLI.node.js --seed 1 <<<'move-to\nline-to\nbezier-to\narc-to\narc-sweep\n'` and paste the generated function into `src/IVG.cpp` as `findPathInstructionType`.
- [ ] Place `findPathInstructionType` alongside the other `/* Built with QuickHashGen */` helpers (around line 515) and invoke it from `PathInstructionExecutor::execute` to switch over the five supported instruction names. Unknown names should return `false` so the interpreter can raise a syntax error.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 3 – basic instructions
- [ ] Within `PathInstructionExecutor::execute`, parse instructions with `IMPD::ArgumentsContainer` and emit drawing commands on the stored `NuXPixels::Path`:
      * `move-to` **must** appear first. Read two numbers and call `path.moveTo(x, y)`.
      * For `line-to`, continue pulling coordinate pairs until the argument list is exhausted and call `path.lineTo(x, y)` for each pair.
- [ ] Add support for `bezier-to`:
      * Fetch the mandatory end point (`x`,`y`).
      * Read the `via:` label – if it contains two numbers call `path.quadraticTo()`, otherwise interpret four numbers and call `path.cubicTo()`.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 4 – arc handling
- [ ] Extract the arc maths used in the `'A'` case of `buildPathFromSVG` (lines 279–334) into a shared helper, e.g. `appendArcSegment(startPos, endPos, rx, ry, xAxisRotation, sweepFlag, largeArcFlag, curveQuality, NuXPixels::Path&)`.
- [ ] Call `appendArcSegment` from the new `arc-to` branch in `PathInstructionExecutor::execute` and also replace the inline code in `buildPathFromSVG` with a helper call to avoid duplication.
- [ ] Implement `arc-sweep` by computing the radius from the current point to the supplied center `(cx,cy)` and delegating to `appendArcSegment` with the calculated end point.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 5 – finalize path and integrate
- [ ] In `IVGExecutor::execute` (around line 1285), when the `svg:` argument is missing, fetch the instruction block as the first unlabeled argument and invoke `buildPathFromInstructions` to construct the path.
- [ ] Parse the optional `closed:` argument in the PATH case; if set to `yes`, call `p.close()` after `buildPathFromInstructions` succeeds.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 6 – documentation
- [ ] In `docs/IVG Documentation.md`, expand the PATH section (starts around line 230) to cover instruction lists, detailing each sub-command with short examples mirroring the ones above.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Milestone 7 – regression test
- [ ] Add `tests/ivg/pathInstructions.ivg` demonstrating all path sub-commands: `move-to`, `line-to`, both `bezier-to` forms, `arc-to`, `arc-sweep`, and a `closed:yes` path. Follow the style of existing tests in `tests/ivg/`.
- [ ] Validate rendering with `bash tools/testIVG.sh tests/ivg/pathInstructions.ivg` and avoid committing generated `.png` files.
- [ ] Run `timeout 300 ./tools/buildAndTest.sh beta native nosimd`.
## Final test
- [ ] `timeout 600 ./tools/buildAndTest.sh beta native nosimd`
