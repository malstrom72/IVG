# Resource Budgets

This describes `IMPD::IMPDBudget` and `IVG::IVGBudget`. The measurements are from release builds on Windows x64. They
were taken on a heavily loaded machine, so read the times as rough.

## Why there are budgets

IVG and IMPD documents are often untrusted: downloaded presets, user skins, files from the web. A document is a
program. Without limits, a few lines can make the host use unbounded memory or time:

| Document | Without a budget |
|---|---|
| `o = x` then `REPEAT 35 [ o = {$o $o} ]` | Asks for a 32 GB string. Reached 21.7 GB before it was killed. |
| `o = x`, `REPEAT 24 [ o = {$o $o} ]`, `REPEAT 10000 [ y = $o ]` | Copies 16 MB per statement and runs for hours at 75 MB. |
| `pen width:1 dash:2` + `rect 0,0,1e90,190` | Builds a 1e8-instruction dashed path: 5.2 GB in 2 s. |
| `bounds 0,0,32767,32767` | Allocates a 4.3 GB canvas. |

The older limits do not cover these:
- The statement count (1,000,000) counts statements, not their size, and 35 statements are enough above. It is now
  part of the budget.
- The recursion and expression nesting limits only protect the stack. They stay on the interpreter, since they guard
  depth, which is refunded on return, not work.
- `PATH_INSTRUCTION_LIMIT`, an earlier fixed limit of a million instructions per path, is gone, and so is
  `MAX_LINE_COORDINATES`, which capped the coordinates of one `line-to`, `LINE` or `POLYGON`; the segment budget
  replaced both.

The goal of the budgets is that every untrusted document either renders, or fails quickly with a clear error, within
memory and time a host can afford. The goal is not to measure memory exactly.

## The design in one paragraph

The budgets count work, not memory, and they count it before the work is done wherever that is possible. The
interpreter charges characters and statements to an `IMPDBudget`. The IVG executor charges path segments and pixels
to an `IVGBudget`, which extends `IMPDBudget`. The host creates one `IVGBudget` per render and passes it to both:

```cpp
IVG::IVGBudget budget;	// or IVGBudget(characters, statements, pathSegments, pixels)
IVG::IVGExecutor executor(canvas, budget);
IMPD::Interpreter impd(executor, vars, formatInfo, budget);
```

The budget is required. There is no default budget and no way to run without one. Every frame the interpreter
creates (CALL, define font, transforms) shares the root's budget. An IMPD-only host passes an `IMPDBudget`. The counts
are public members, so a host can read what is left after a render, or measure a document by passing a huge budget.

Going over a count throws an `IMPD::RunTimeException`:
- "Characters limit reached.";
- "Statements limit reached.";
- "Path segment budget exceeded.";
- "Pixel budget exceeded.".

The counts are cumulative over the whole render. Nothing is refunded when memory is freed, so each count bounds the
total work of its kind. Memory is then bounded indirectly: nothing can hold more data than was charged to build it,
times a small factor. The largest is a list split into elements (`FOR ... IN`, and the coordinates of `line-to`,
`line-angle`, `LINE` and `POLYGON`): each element costs about 30 bytes on top of its characters, so the default
budget lets one list take about 250 MB.

## IMPDBudget: statements

`run()` charges one statement before each statement runs, and the executor's `progress()` hears how many are left,
counting the current one, as before. Default: `IMPD::DEFAULT_STATEMENTS_LIMIT` = 1e6. It bounds how long loops and
recursion can run, but not how much each statement does; the character count covers that.

## IMPDBudget: characters

### What is charged

There are four points in `src/IMPD.cpp`, all in `Interpreter`:

1. `run(r)` charges the length of the text it is about to scan. This covers loops that re-run long blocks,
   including blocks that are only comments.
2. `performExpansion` charges the length of its result. Every statement is expanded before it runs, as are the
   `[ ]` arguments executors expand, so this covers statement text and `$var` and `{expr}` output.
3. `get(name)` charges the length of the value it copies out of a variable. This covers `$x` inside expressions,
   where a big value can be read and then cut down by a substring.
4. `concatOperation` charges the lengths of both operands, before joining them.

These are where IMPD creates strings whose size is not already bounded by charged text. Everything else IMPD does
with strings is proportional to text already charged: substrings, assignment, argument and list parsing, unescaping,
tracing, error messages. Statement text is charged about twice, once when scanned and once when expanded.

Default: `IMPD::DEFAULT_CHARACTERS_LIMIT` = 1e8. Measured on the 135 `.ivg` files in the repository, the most any
document uses is 4.4 million (`bitbox_logo.ivg`), and the fuzz corpus uses at most 42 million. In practice a flat
document costs about two to three times its own size.

### What it bounds

- **Time on strings:** about 0.3 to 0.5 s of string work at the default (about 3e8 characters per second).
- **Memory in strings:** every doubling and copy-loop input from the fuzzer now stops within 0.2 s at 37 to 75 MB,
  where before it used 2 to 21 GB or ran for hours.

### What it does not capture (known amplification)

These cost memory or time beyond the characters charged, by a bounded factor:

- **Per-object overhead.** Only characters are counted, not the objects that hold them:
  - Each variable is a map node of about 100 bytes. The statement limit caps this at about 100 MB.
  - Each list element (`FOR x in:`, argument lists) is a string object of about 32 bytes, so a list of one-character
    elements takes about 16 times its text, an estimated 0.4 GB at most at the default.
- **Wide strings.** `unescapeToUni` and `unescapeToWide` produce 4 or 2 bytes per character, for text, trace and
  file names.
- **Separate root interpreters.** A host or IVG may create its own root interpreter, and it uses whatever budget is
  passed to it:
  - IVG's color helper parses one color string with its own small budget;
  - IVG2PNG and the IVGFiddle rasterizer pass the render's budget when they load an external font file, so the font
    counts toward the render.

### What could go wrong

- **Large legitimate documents can fail.** Cost grows with document size, and with block size times loop count.
  These would reach 1e8:
  - a generated document of about 40 MB or more;
  - a 25 KB block drawn 2000 times in a loop.

  Such documents usually reach the IVG segment budget first. The error message says which limit was reached, and the
  host can pass a larger budget.
- **Counts depend on the implementation.** If IMPD is refactored, for example to expand a statement once less, a
  document's count changes, and one near the limit may pass or fail differently between library versions.
  `IMPDTest` pins an exact count so such changes are seen.
- **New features must charge.** Any new IMPD feature that builds strings (a new function, a string operator, a native
  loop) must charge, or it opens a new hole. Rule for maintainers: build new strings through the charge points above,
  or charge explicitly before the work. The IMPD fuzz target is the safety net.
- **Charges outside a render.** A host that calls `get()` or `expand()` after a render is charged too. If the budget
  is nearly used up, a read that should succeed throws. No current host does this.
- **Budget reuse.** Counts are cumulative, so a host that reuses one budget for several renders, or one interpreter
  for many scripts, runs out. Create a new budget per render. The statement limit already works this way.
- **No partial result.** An exceeded budget fails the whole document, like any run-time error. Products should show
  a fallback image.

## IVGBudget: path segments and pixels

### What is charged

Path segments, in `Context::stroke` and `Context::fill`, before rasterizing:
- the input path;
- the dashed path;
- the stroked outline.

`Path::dash` and `Path::stroke` take the remaining budget as a limit and stop there. `svg` path data, text, `path [ ]`
blocks and coordinate lists (`line-to`, `line-angle`, `LINE`, `POLYGON`) stop building once they exceed the remaining
budget, and the path is then refused. A `define path` is charged its segments once when it is defined, since it
stays in memory, and again each time it is drawn.

Pixels:
- the clipped bounding box of each fill, stroke and image;
- the canvas or mask area for a `wipe`;
- the raster area of every `bounds` (canvas, image definition, pattern, scaled by resolution), before it is
  allocated;
- the canvas area for each `mask`, including `mask invert`.

Defaults: `IVG::DEFAULT_PATH_SEGMENTS_LIMIT` = 1e7 segments and `IVG::DEFAULT_PIXELS_LIMIT` = 5e8 pixels. Real
documents use at most 85,000 segments (`gammaTest.ivg`) and 278 million pixels (`huge.ivg`, a stress test; the next
largest is 13 million). That holds for IVG-3 too: of the 102 documents in `tests/ivg`, 40 are IVG-3, and none of them
goes past these peaks.

### What it bounds (measured at the defaults)

| Document | Result |
|---|---|
| `pen width:1 dash:2` + `rect 0,0,1e90,190` | Refused after 0.2 s at 460 MB. |
| `bounds 0,0,32767,32767` | Refused at once. |
| `bounds 0,0,22000,22000` (largest canvas allowed) | Renders, 1.85 GB. |

### What it does not capture

- **Rasterizing time grows with overlap, faster than the segment count.** The rasterizer's work is roughly segments
  times the rows each spans, plus sorting of edges that cross. A stroke with round caps 1000 px wide on a dashed
  `rect` (five lines of document) measured:
  - 1 million segments: 0.9 s;
  - 2 million segments: 3.3 s;
  - 1e7 segments (allowed by the default): still running after 60 s, estimated 1 to 2 minutes.

  The segment and pixel counts are each within budget, and their product is not limited.
- **Memory per segment is about 130 bytes when filling.** That is the path, its transformed copy and the rasterizer's
  segment and edge tables. A 1e7-segment fill is therefore about 1.3 GB. Together with the largest canvas, a document
  reaches about 3 GB, measured at 2.6 GB after 60 s. The "about 2 GB" target holds for each route alone, not combined.
- **Bounding boxes overcharge.**
  - A thin diagonal line or a ring outline is charged its whole bounding box, though rasterizing it costs far less.
  - Each `mask` is charged the whole canvas.
  - Pixels scale with the square of the render scale.

  A legitimate document rendered at high resolution, with many full-canvas elements, can reach 5e8: for example
  about 125 full-canvas fills or masks at 2000 x 2000. This is the most likely way the IVG budget could reject real
  product art, and it should be measured on product documents at their real render scales before the defaults are
  frozen.
- **Host-loaded images.** An image a host decodes in `loadImage` is not charged until it is drawn. Its decoding and
  memory are the host's responsibility.
- **Per-pixel cost varies.** Gradients, textures, gamma and masks cost more per pixel than solid fills. The pixel
  count bounds them, but not uniformly.
- **Segment counts depend on scale and quality.** Curve subdivision follows the transform and `curve-quality`, up to
  200 segments per circle and 200 per spline, so the same document costs more at larger scales.

### What could go wrong

- **Product art failing at high resolution:** see the overcharging above. This is the main risk.
- **Version drift:** a change in NuXPixels curve subdivision or stroking changes segment counts, and with them which
  documents fit.
- **Combined limits are not a memory cap:** as above, a document can combine the worst cases of each count.
- **Not thread safe:** one budget per render, on one thread.

## Guidance for hosts

- Create one `IVGBudget` per render, pass it to both the executor and the interpreter, and discard it afterwards.
  Both keep a reference to it, so it must outlive them: declare it before them, never pass a temporary.
- Choose budgets for your memory and your documents. Measure your own documents at your largest render scale. The
  defaults suit a desktop with several GB to spare. Use smaller budgets on mobile or WebAssembly:
  - the fuzz harness uses 5e6 segments and 2.5e8 pixels, to stay within libFuzzer's 2 GB;
  - IVGFiddle uses 1e7 characters, 2.5e5 segments and 5e7 pixels, for its fixed 64 MB heap.

  The pixel count is cumulative painting, not the size of a raster, so a budget alone cannot keep every allocation
  within a small heap. Catch `std::bad_alloc` as well.
- Catch `IMPD::Exception` for budget and document errors, and `std::exception` for `std::bad_alloc`. A budget bounds
  work, not the heap, so a small heap can still run out within budget.
- Treat images you decode in `loadImage` as outside the budget.

## Guidance for maintainers

- Charge before the work wherever possible. Where the size is only known afterwards, as with stroke output or
  expansion results, make the producer stop at the remaining budget (`Path::dash` and `Path::stroke` take a limit).
- Any new way to build strings in IMPD, or paths or rasters in IVG, needs a charge and a `BudgetTest` or `IMPDTest`
  case, exact counts where practical.
- Keep the fuzz targets running on changes. Out-of-memory and timeout reports are how holes show up.
