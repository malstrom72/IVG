# Flat IVG and ImpD-0

Design proposal for the next revision of **IVG-3**: split ImpD into two specification layers
(**ImpD-0** syntax, **ImpD-1** language), define a **flat IVG** profile marked by
`requires:ImpD-0`, add a **flattener** that compiles full IVG down to that profile, and build a
suite of **converters** (SVG, JS canvas, Core Graphics, Direct2D) as plain-JS emitters that
consume flat IVG.

Status: design discussion, not yet implemented. Since IVG-3 is effectively unpublished and the
existing file corpus is fully under our control, all changes below can be made without
backward-compatibility machinery — this is the moment to fold them into IVG-3 itself.

## Motivation

The goal is a suite of converters *from* IVG to other vector targets:

- **SVG**
- **JS canvas** (generated `ctx.*` drawing code)
- **Core Graphics** (generated `CGContext` code)
- **Direct2D** (generated code; chosen over GDI+, see [Target notes](#target-notes))

Because ImpD is a real imperative language (variables, expressions, loops, procedures), an IVG
file cannot be statically translated. It must be *executed*, with the resulting draw operations
recorded — the same model as PostScript → PDF, or Skia's `SkPicture`. This leads to a two-stage
architecture where all converters share one hard component:

```
                    (C++, in core)                    (plain JS, in tools/)
  full IVG  ──────►  flattener  ──────►  flat IVG  ──────►  emitters  ──────►  SVG
 (ImpD-1)         partial evaluator     (ImpD-0)         transliterators       canvas JS
                                                                               CG code
                                                                               D2D code
```

- **Stage 1 — flattener.** Runs the existing interpreter once and records every resolved draw
  operation (path, transform, paint, stroke, mask), serialized as *IVG itself* — a trivial
  subset with no variables, no expressions, no control flow. Loops come out unrolled,
  procedures inlined.
- **Stage 2 — emitters.** Each target is a small, isolated program that parses the flat
  dialect and prints target code. Since the input has no control flow and no state tricks, an
  emitter is a dumb transliterator — a few hundred lines of string formatting.

Key properties:

- ImpD semantics live in exactly **one implementation** (the existing C++ interpreter). No
  second interpreter to drift out of sync.
- Flat IVG is still valid IVG, so the flattener is verified with the existing golden-PNG
  infrastructure: render original, render flattened, image-diff — *same rasterizer, near-exact
  match required*. Every `.ivg` in the test corpus becomes a flattener test case.
- Emitters can be **plain JS**, dependency-free and NuXJS-compatible: the same file runs under
  Node (CLI tools, test suite), in the browser (IVGFiddle export, via the existing
  `rasterizeIVG` WASM build for the flattener), and embedded in native tools via NuXJScript.
- Adding a new target later touches nothing existing.

## The ImpD-0 / ImpD-1 split

"ImpD" today names both the statement/argument *syntax* and the *language* on top of it. Flat
IVG keeps the entire syntactic substrate and drops only evaluation. The split is therefore made
official as two specification layers sharing one version sequence:

| Layer | Contents |
|---|---|
| **ImpD-0** | Statement syntax: one statement per line, instruction + arguments, labeled arguments (`label:value`), lists, quoted strings, bracket blocks, comments. Primitive instructions: `format`, `meta` only. No evaluation of any kind. |
| **ImpD-1** | Everything else, layered on ImpD-0: variables and assignments, `$` substitution, `{...}` expressions, primitive functions, and the primitive instructions `call`, `for`, `if`, `include`, `local`, `repeat`, `return`, `stop`, and (proposed) `trace`, `debug`. |

Notes:

- ImpD-1 **implies** ImpD-0 — the language is a strict superset of the syntax layer, so every
  flat file is also a valid ImpD-1 file.
- `trace` and `debug` are proposed for ImpD-1: flattener output never contains them and a pure
  data format has no business tracing. (Open question below.)
- The ImpD documentation restructures along the same line: an "ImpD-0: data syntax" chapter
  (roughly one page) and an "ImpD-1: evaluation" chapter. The flat IVG standard references only
  the ImpD-0 chapter, so a third-party flat renderer never needs to read a language spec.
- The analogy to keep in mind is **JSON vs. JavaScript**: a data format whose grammar is a
  subset of a language's, specified standalone, with the heritage as a compatibility note
  rather than a dependency.

### The `requires:` mechanism

The existing negotiation mechanism already has the right semantics — "reject me if you can't
provide this":

- `IMPD.cpp` (`format` handling, ~line 1348) currently filters the single supported ImpD id out
  of the declared `requires:` set.
- `IVG.cpp` (~line 1253) rejects the file if any requirement remains unfiltered.

**Change:** the interpreter's single `CURRENT_IMPD_REQUIRES_ID` becomes a *set of satisfied
ids*. A full engine satisfies both `impd-0` and `impd-1` and loads everything; a flat-only
parser satisfies only `impd-0`, so `requires:ImpD-1` files bounce off it with a clean format
error. Both directions of negotiation fall out of existing code paths.

### Declaration semantics

| Header | Meaning |
|---|---|
| `format IVG-3 requires:ImpD-1` | Full IVG: procedural authoring layer. What humans write. |
| `format IVG-3 requires:ImpD-0` | Flat IVG: interchange profile. What the flattener emits and emitters consume. |
| `format IVG-3` (no `requires:`) | **Proposed:** implies `requires:ImpD-0` — data by default, language opt-in. |
| No `format` line at all | **Proposed:** today's permissive full-ImpD mode, so quick sketches (IVGFiddle) stay frictionless. Formality kicks in only on declaration. |

### Enforcement

Today `requires:ImpD-1` is an unchecked claim: the interpreter happily runs variables and loops
in files that never declared it. A survey of the test corpus (190 `.ivg` files) shows the
convention is currently meaningless — 55 files say bare `format IVG-3`, ~47 declare
`requires:IMPD-1`, 51 have no format line, and the declaration does not correlate with actual
ImpD usage.

**Change:** the interpreter enforces the declared level. In ImpD-0 mode, all ImpD-1 features
(`$`, `{...}`, assignments, control-flow instructions, `include`) are syntax errors. This makes
the marker self-correcting: writing `$x` in an undeclared file produces an immediate error
telling the author to add `requires:ImpD-1`. Accidental misclassification cannot survive
enforcement. The ImpD-0 allowlist *is* the enforcement table *is* the flat spec's instruction
inventory — writing one produces the others.

**Migration (one-time, mechanical):** files already declaring `requires:IMPD-1` are correct;
the 55 bare `format IVG-3` files either gain the clause (if they use language features) or are
already flat; no-format-line sketches are untouched. Do this as part of the IVG-3 finalization,
while the corpus is small and fully ours.

## The flat IVG profile

Flat IVG is not a new format — it is IVG-3 restricted to ImpD-0 plus the drawing model. Rules:

1. **No evaluation.** No variables, expressions, control flow, includes. Parse time and render
   time are proportional to file size (bounded execution — the property that makes the format
   safe to embed and trivial to analyze statically).
2. **No parameters, no recursion.** Nested *blocks* are allowed where the drawing model needs
   them (mask content, pattern content); parameterized reuse is not. Every concession toward
   `<defs>`/`<use>`-style referencing is a step back toward being a language. Compactness of
   unrolled output is delegated to gzip, which handles repetition extremely well.
   (Whether non-parameterized `define path` / `define font` / `define image` — IVG
   instructions, not ImpD — are allowed in flat files is an open question below.)
3. **Minimal graphics state machine.** Flat IVG keeps IVG's inherit/push/pop context state
   (transform, fill, pen, etc.), same as PDF keeps `q`/`Q`. A state machine is not a language,
   and it is what keeps output compact and maps 1:1 onto every target's save/restore layers.
4. **Canonical serialization.** The flattener controls the output, so the profile can guarantee
   a canonical form: one instruction per line, no continuations, fixed argument order, fixed
   number formatting, no escaping surprises. Emitters then need only a ~100-line reader, not an
   ImpD parser.

### Flattening semantics

Resolution rules applied by the flattener so emitters stay dumb:

- **Text → glyph paths.** `Font::Glyph` already stores each glyph as an SVG path string, so
  text expands to paths at flatten time. No emitter ever needs font support.
- **Relative paints → absolute** (bounds are known at flatten time).
- **Strokes stay symbolic** (width, caps, joints, miter limit, dash) rather than being
  pre-flattened to fill outlines. All targets stroke natively; symbolic strokes keep output
  compact and editable.
- **Masks and patterns → nested blocks** of vector content. All targets have push/pop layer
  semantics (SVG `<g>`/`<mask>`, canvas save/offscreen composite, CG GState/transparency
  layers, D2D `PushLayer`), so a stream with begin/end group markers is the common denominator.
- **Gamma / AA are rasterizer traits**, not format content: targets do their own AA, so
  converted output is compared with a tolerance, never pixel-exact.

## Core renderer changes

The footprint in the C++ core is deliberately small:

1. **Curve-preserving path type** — the one genuinely invasive change. `NuXPixels::Path`
   flattens curves at build time (its operation set is `MOVE`, `LINE`, `CLOSE`;
   `cubicTo`/`quadraticTo` subdivide immediately). Hooking a recorder at `Context::fill` /
   `Context::stroke` today would emit polygon soup. Fix: a curve-retaining path type
   (move/line/quadratic/cubic/close) constructed by the shape builders and
   `buildPathFromInstructions`, flattened to `NuXPixels::Path` at the last moment inside
   `Context` when actually rasterizing. Rendering behavior must not change at all — fully
   covered by the existing goldens.
2. **Recording canvas/device** alongside `MaskMakerCanvas` (which already establishes the
   alternative-canvas pattern), appending resolved ops instead of blending pixels.
3. **Flat IVG serializer** implementing the canonical form.
4. **`requires:` satisfied-set** change in `IMPD.cpp` (a few lines) plus ImpD-0 strict mode.
5. **WASM export**: a `flattenIVG` entry point next to the rasterizer in
   `tools/ivgfiddle/src/rasterizeIVG.cpp`, so the browser gets the flattener for free.

Explicitly *not* proposed: an abstract multi-backend device interface threaded through
`IVGExecutor`. Backends-as-virtual-dispatch is the unmaintainable version of this project; the
flat format is the backend interface.

## Emitters

Each emitter is one file, plain JS, dependency-free, NuXJS-compatible (conservative JS level,
no Node APIs in core logic, string in → string out, I/O at the edges). Suggested build order:

1. **SVG** — easiest and highest value. Vector-to-vector; IVG path syntax is already SVG path
   syntax, so curve data passes through nearly verbatim. Masks and gradients map well.
   Together with `svg2ivg` this gives round-tripping.
2. **JS canvas** — the Canvas 2D API is imperative, so the flat stream maps almost 1:1
   (`beginPath`/`bezierCurveTo`/`fill`). Soft masks via offscreen canvas +
   `globalCompositeOperation = "destination-in"`.
3. **Core Graphics** — procedural `CGContext` dialect of the canvas emitter;
   `CGContextClipToMask`, transparency layers, `CGPattern`.
4. **Direct2D** — modern Windows target: layers with opacity masks, proper radial gradients.

### Target notes

- **GDI+ was considered and dropped**: no soft/alpha masks, `PathGradientBrush` radial
  semantics differ, patterns are raster-only `TextureBrush` — rasterized fallbacks for exactly
  the features that make IVG nice. Direct2D is what current Windows code uses. Revisit only if
  a concrete legacy consumer appears.
- These are **code generators** (PaintCode-style: output a drawing function to paste into an
  app), not runtime backends. A live D2D/CG rendering backend is a different, much larger
  project — but the flat op stream is the right substrate for one later, so nothing is lost by
  deferring.
- Output is **unrolled**: a loop drawing 200 stars becomes 200 elements. That is compilation,
  not translation; procedural structure does not survive, by design.

## Testing strategy

| Component | Verified by |
|---|---|
| Flattener | Render original vs. flattened through the existing rasterizer; near-exact golden-PNG diff. Whole corpus, existing infra. |
| Curve-path refactor | Existing goldens unchanged (pure refactor, no rendering change). |
| SVG emitter | Existing resvg comparison path (`docs/resvg-tests-coverage.md`, Node harness). |
| Canvas emitter | Execute generated code in node-canvas; render original via the WASM module; image-diff with tolerance. Pure Node. |
| CG / D2D emitters | Small native render-to-bitmap harnesses on their platforms; tolerance diff. |
| NuXJS compatibility | CI runs each emitter under NuXJScript itself, so compatibility is enforced rather than aspirational. |
| Flat profile conformance | The flattened corpus + golden PNGs *is* the conformance kit for third-party implementations. |

## Open questions

1. **`define` in flat files.** `define path`/`font`/`image` are IVG instructions (not ImpD) and
   are non-parameterized, so they could legitimately appear in the flat profile — the
   `<defs>`/`<use>` question in another form. Leaning: exclude from flat v1 (flattener inlines
   everything), keep the door open.
2. **`trace`/`debug` placement.** Proposed ImpD-1; harmless enough that ImpD-0 could keep them
   as diagnostics. Decide when writing the ImpD-0 chapter.
3. **Bare `format IVG-3` default.** Proposed to imply ImpD-0. Alternative: always require an
   explicit clause in declared files. Affects how noisy sketches are.
4. **No-format-line semantics.** Proposed to stay permissive (full ImpD). Alternative: default
   to ImpD-1 semantics explicitly documented rather than "legacy mode".
5. **Version tag for the profile.** Flat files are just IVG-3 + `requires:ImpD-0`; the informal
   name "flat IVG" appears only in documentation. Is a `meta` self-description (e.g.
   `meta profile …`) still wanted for tooling display, or is the requires clause enough?
6. **Stroke transform semantics.** Pin down exactly how stroke width interacts with the CTM in
   the flat spec (pre- vs. post-transform), since emitters must reproduce it on targets with
   differing native behavior.
7. **Miter-limit and dash-phase dialects.** Definitions differ slightly across targets
   (e.g. miter limit conventions); the flat spec should define IVG's semantics precisely and
   emitters compensate.

## Suggested work order

1. `requires:` satisfied-set + ImpD-0 strict mode + corpus migration (small, independent,
   locks in the spec direction).
2. Curve-preserving path type + `Context` seam (pure refactor, goldens must not move).
3. Recording device + canonical flat serializer; verify flatten(x) ≈ x across the corpus.
4. ImpD-0 chapter + flat profile spec (the allowlist falls out of step 1, the canonical form
   out of step 3).
5. SVG emitter + Node tests; wire `flattenIVG` into the WASM build and an IVGFiddle export
   button.
6. Canvas emitter + node-canvas tests.
7. CG emitter, D2D emitter + native harnesses.
