# PATH Roadmap

## PATH Instruction Overview

PATH draws an arbitrary vector path filled with the current `fill` setting and outlined with the current `pen` setting. The command accepts an instruction list, raw SVG data, or the name of a previously defined path.

### Syntax

```
PATH (<instructions> | svg:<svg data> | <name>) [transform:<transform>]
```

- `<instructions>` – bracketed sub-commands separated by new lines or semicolons.
- `svg:<svg data>` – SVG path string; see <https://svgwg.org/specs/paths/>.
- `<name>` – identifier supplied by `define path`.
- `transform:` – same transformation syntax as `IMAGE`, applied before drawing.

The `svg:` form is supported in all IVG versions; the instruction-list variant requires IVG‑3.

### Path Instructions

- `move-to <x>,<y>` – sets the starting point for a new sub-path.
- `line-to <x>,<y>[,<x>,<y> ...]` – draws one or more line segments from the current point.
- `bezier-to <cx>,<cy>,<x>,<y>` – draws a quadratic Bézier curve.
- `bezier-to <c1x>,<c1y>,<c2x>,<c2y>,<x>,<y>` – draws a cubic Bézier curve.
- `arc-to <x>,<y>,<r>[,<ry>=<r>] [turn:cw|ccw=cw] [large:yes|no=no] [rotate:<deg>=0]` – draws an elliptical arc.
- `arc-sweep <cx>,<cy>,<degrees>` – draws an arc around a center point, sweeping by the given angle.
- `arc-move <cx>,<cy>,<degrees>` – moves the current point along an arc sweep without drawing.
- `anchor [<x>,<y>]` – sets a new local origin. Without arguments it uses the current point; with coordinates it interprets them as a global point.
- `cursor <var>` | `cursor ( [x:<var>] [y:<var>] )` – stores the absolute cursor position into variables. For example, `cursor p` sets `$p = "x,y"` and `cursor [x:px y:py]` sets `$px` and `$py`. At least one of `x:` or `y:` must be present.
- `close` – closes the current sub‑path by drawing a line back to its starting point.

#### Rules

- Commands that require a current point (`line-to`, `bezier-to`, `arc-to`, `arc-sweep`, `arc-move`, `close`) error if none exists.
- A top‑level `PATH [...]` usually begins with `move-to`. Inline fragments inserted via `path` may omit it and inherit the caller’s current point.

### Sub-path Commands

These mirror drawing commands but only append geometry (no paint) and may appear inside `PATH [...]`:

- `line <x0>,<y0>,<x1>,<y1>[,<x2>,<y2> ...]` – appends an open polyline starting at `<x0>,<y0>`.
- `rect <x>,<y>,<w>,<h> [rounded:<r>|<rx>,<ry>]` – appends an axis‑aligned rectangle.
- `ellipse <cx>,<cy>,<r>[,<ry>=<r>] [sweep:<start>,<degrees> [type:(pie|chord)=chord]]` – appends a full ellipse or a closed sector.
- `star <cx>,<cy>,<points>,<r1>[,<r2>=<r1>] [rotation:<angle>]` – appends a star or regular polygon.
- `polygon <x0>,<y0> <x1>,<y1> [<x2>,<y2> ...]` – appends a closed polygon.
- `text [at:<x,y>] [anchor:left|center|right=left] <text>` – appends a text outline.
- `path <name> | svg:<data> | [<instructions>] [transform:<transform>]` – splices another path’s geometry into the current `PATH`.
	- `<name>` – a path defined with `define path`.
	- `svg:<data>` – SVG path string.
	- `[<instructions>]` – inline `PATH` fragment.
	- `transform:` – applies before insertion.
	- If the fragment starts without `move-to`, it continues from the caller’s current point.
	- After insertion, the caller’s current point is updated to the end of the fragment.

## Differences from current implementation
- Remove the `closed:` option handled by `IVGExecutor::buildPath` in `src/IVG.cpp` lines 1273‑1282 and rely on an explicit `close` instruction instead.
- Introduce `anchor` to set a new local origin inside a path; currently `PathInstructionExecutor` in `src/IVG.cpp` (around line 650) offers no such offset.
- Introduce `cursor` to store the current point in variables; the existing executor exposes no instruction that writes `path.getPosition()` to the `Interpreter`.
- Allow `path` sub‑commands to splice other paths, SVG data, or inline fragments; the current switch in `PathInstructionExecutor::execute` (`src/IVG.cpp` lines 662‑755) has no `path` case.
- Enforce the initial `move-to` requirement only when drawing so `define path` can contain fragments. Validation occurs at draw time in `IVGExecutor::execute` (`src/IVG.cpp:1698-1703`).
- Remove the `end:` parameter from `arc-sweep` and `arc-move` (`src/IVG.cpp` lines 736‑751).
- Reuse `NuXPixels::Path::append` and `transform` (`externals/NuX/NuXPixels.cpp:380` and :916‑919) for geometry splicing instead of writing custom logic.

## Milestones

### Milestone 1 – Parser and Syntax Updates
- [x] Extend `findPathInstructionType` (`src/IVG.cpp:543`) and the `Type` enum (`src/IVG.cpp:635`) to recognize `anchor`, `cursor`, `path`, and `close`; regenerate the token hash table with `node externals/QuickHashGen/QuickHashGenCLI.node.js`.
- [x] Move `checkHasMoveTo` so fragments defined by `define path` are accepted: remove parser-time checks and validate after interpretation in the `PATH` case of `IVGExecutor::execute` (`src/IVG.cpp:1698-1703`).
- [x] Run `timeout 600 ./build.sh` and record any failures for follow‑up in Milestone 2.

### Milestone 2 – Anchor and Cursor Semantics
- [x] Add a `Vertex anchorOrigin` member to `PathInstructionExecutor` (`src/IVG.cpp:650`).
- [x] Initialize `anchorOrigin` to `Vertex(0,0)` in the constructor.
- [x] Handle `PATH_ANCHOR_INSTRUCTION` in `execute`:
- [x] No arguments – set `anchorOrigin = path.getPosition()`.
- [x] With `<x>,<y>` – parse using `parseNumberList` and assign to `anchorOrigin`.
- [x] Offset geometry relative to `anchorOrigin`.
- [x] Add helper `applyAnchor(double& x, double& y)` that adds `anchorOrigin`.
- [x] Use the helper in `moveTo`, `lineTo`, `quadraticTo`, `cubicTo`, `arcSweep`, and `arcMove` cases.
- [x] Translate appended geometry (`line`, `rect`, `ellipse`, `star`, `polygon`, `text`) in `appendChecked`.
- [x] Implement `cursor` support in `PathInstructionExecutor::execute`.
- [x] Fetch `path.getPosition()` and format `"x,y"` with `impd.toString`.
- [x] `cursor p` – assign the combined string with `impd.set`.
- [x] `cursor [x:px y:py]` – assign individual variables when present.
- [x] Add regression tests under `tests/ivg` exercising `anchor` and `cursor`.
- [x] Run `timeout 600 ./build.sh`; address new failures or carry them forward.

### Milestone 3 – Path Splicing and Transform
- [x] Add a `path` case to `PathInstructionExecutor::execute` (switch at `src/IVG.cpp:662‑755`).
- [x] Plain name – look up `IVGExecutor::definedPaths` (`src/IVG.h:377`).
- [x] Inline fragment or `svg:` – call `IVGExecutor::buildPath` to create a temporary `Path`.
- [x] Apply optional `transform:` before merging.
- [x] Clone the source geometry.
- [x] Use `NuXPixels::Path::transform` (`externals/NuX/NuXPixels.cpp:916‑919`).
- [x] Merge geometry into the current path.
- [x] Translate by current `anchorOrigin` so nested geometry lands in the caller’s coordinate system.
- [x] Append with `NuXPixels::Path::append` (`externals/NuX/NuXPixels.cpp:380`).
- [x] Rely on `Path::getPosition()` to advance the cursor automatically.
- [x] Permit `define path` to contain fragments by accepting paths without an initial `move-to`.
- [x] Create tests demonstrating path splicing with and without transforms.
- [x] Run `timeout 600 ./build.sh`; resolve issues or note them for Milestone 4.

### Milestone 4 – Close Handling and Legacy Cleanup
- [x] Implement the `close` instruction in `PathInstructionExecutor`.
- [x] Call `path.close()` (`externals/NuX/NuXPixels.cpp:386`).
- [x] Reset `anchorOrigin` to `(0,0)` so subsequent segments use global coordinates.
- [x] Retire legacy options.
- [x] Remove `closed:` parsing in `IVGExecutor::buildPath` (`src/IVG.cpp:1273-1282`).
- [x] Drop the `end:` parameter from `PATH_ARC_SWEEP_INSTRUCTION` and `PATH_ARC_MOVE_INSTRUCTION` (`src/IVG.cpp:736‑751`).
- [x] Refresh documentation and tests to match the new syntax.
- [x] Update `docs/IVG Documentation.*` and examples.
- [x] Adjust regression tests under `tests/ivg`.
- [x] Run `timeout 600 ./build.sh`; remaining failures are targeted for the final milestone.

### Milestone 5 – Final Integration
- [x] Resolve outstanding test failures and ensure compatibility with existing `.ivg` files.
- [x] Validate specification compliance, formatting (tabs, 120-column limit), and update any remaining docs.
- [x] Run `timeout 600 ./build.sh` expecting a clean pass.
