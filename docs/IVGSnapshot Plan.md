# IVGSnapshot System Plan

## Objectives
- Deliver a standalone C++ regression harness (`IVGSnapshot`) that validates `.ivg` snippets by rendering them to PNG and comparing them with stored goldens.
- Support inline opt-in metadata inside IVG sources via `meta snapshot [...]` blocks so designers can control parameters, validation gates, and other execution details without affecting runtime renderers.
- Provide a fast inner loop for artists: they can temporarily disable validation, iterate in ivgfiddle or their host product, then re-enable validation to lock the expected output.

## High-Level Workflow
1. Discover the IVG inputs provided on the command line, normalize their paths, and locate companion golden files in a deterministic folder structure (default: same directory with `.png`).
2. Parse each IVG file looking for the most specific `meta snapshot` block. Fall back to default settings when metadata is missing.
3. Resolve ImpD variables declared in the meta block to build a concrete render configuration (canvas size, parameter set, golden policy, etc.).
4. Invoke the IVG renderer (shared engine from `ivg2png`) with the configured parameters plus include/font/image search paths from CLI flags.
5. Write the rendered PNG to a temp location, compare it bytewise to the golden (or manage `png.disabled` sentinel), and summarize the verdicts.
6. Respect force-update flags by refreshing goldens with the new renders while still reporting differences.

## Command-Line Interface
- `IVGSnapshot [options] <ivg> [<ivg> ...]`
- Options:
  - `--include-dir <path>` (repeatable): augment search paths for `include "..."` directives.
  - `--font-dir <path>` (repeatable): supply font lookup directories, matching ivg2png semantics.
  - `--image-dir <path>` (repeatable): supply bitmap lookup directories.
  - `--output-dir <path>`: optional override for generated PNGs (default: alongside IVG).
  - `--force-update`: replace goldens with newly rendered output regardless of diff status.
  - `--threads <n>`: cap parallel render concurrency.
  - `--list-only`: dry-run reporting of discovered files and meta configs.
  - `--verbose`: echo detailed diagnostics, including resolved parameter sets and comparison hashes.
  - `--exit-on-first-failure`: support CI scenarios that want early termination.

## File Layout & Golden Handling
- Golden naming: `<basename>.png` stored next to the IVG or inside a mirror directory under `output/snapshot/` (configurable).
- Disabled sentinel: `<basename>.png.disabled` lives next to the IVG. When present, it signals intentional suppression and the
  harness skips comparison.
- On `validate:no` meta:
  - If a golden PNG exists, rename it to `.png.disabled` (replacing any prior sentinel) so the latest render is preserved.
  - Retain the `.png.disabled` sentinel across runs and report that the scenario is in draft mode.
  - Absence of both PNG and sentinel is acceptable; the harness notes the skipped comparison.
- On `validate:yes` meta:
  - If a golden PNG exists, the render must match byte-for-byte; differences fail the test.
  - If the PNG is missing but a `.png.disabled` sentinel exists, render a fresh golden, delete the sentinel, then compare.
  - If both PNG and sentinel are missing, treat as an error unless `--force-update` is passed, in which case the golden is
    created before comparison.
  - Always ensure no `.png.disabled` sentinel remains once validation is re-enabled.
- Default behavior (no meta): treat as `validate:yes` with implicit bounds derived from the render output.

## Meta Block Semantics
- Grammar: `meta snapshot <key:value pairs> [ <ImpD statements> ]`.
- Recognized keys:
  - `validate:(yes|no)`: toggle golden expectations.
  - Arbitrary parameter assignments (e.g., `size=5; color=green; text="foo"`) evaluated as ImpD expressions and injected into the interpreter environment before playback.
- Execution flow:
  1. Parse doc up to meta block using existing lexer (reuse from IVG parser if possible).
  2. Evaluate the ImpD chunk using embedded interpreter from IVG runtime to seed global variables.
  3. Track multiple `meta snapshot` blocks and select the one closest to the top of file unless a `name:"scenario"` key allows multiple scenario runs.
- Extensibility: allow arrays/maps to support future parameter sweeps.

## Rendering Engine Integration
- Reuse the core rendering stack already used by `ivg2png` (ImpD evaluator + IVG rasterizer).
- Create a library facade (`libIVGSnapshot`) with hooks:
  - `LoadIVG(const char* path, SnapshotMetadata&)` -> parse document, extract metadata.
  - `RenderIVG(const RenderConfig&, Surface&)` -> run the interpreter, produce raster.
  - `WritePNG(const Surface&, const char* path)` -> share with ivg2png to avoid duplication.
- Support optional headless glyph caching per run to avoid repeated font shaping costs.
- Manage include/image/font resolution through a shared resource manager honoring CLI search paths.

## Comparison Strategy
- Load goldens and render output into RGBA surfaces (premultiplied expected).
- Verify dimensions match; mismatch is an immediate failure (report expected vs. actual sizes).
- Perform pixel-by-pixel comparison; collect:
  - First differing coordinate.
  - Count of mismatched pixels.
  - Max absolute channel delta.
- Provide tolerance options for floating errors if needed (default zero tolerance).
- For failures, write diff artifacts (`.diff.png`, `.actual.png`) to assist debugging.

## Force Update Mechanics
- When `--force-update` is set:
  - Always write the rendered output to the golden path (unless `validate:no`).
  - Keep previous golden as `.bak` for rollback (optional flag `--keep-backup`).
  - Still report whether the prior golden differed to inform developers.
- When not set:
  - Only update goldens when transitioning from `validate:no` to `validate:yes` or if the golden is absent and not disabled (then fail instead of auto-create).

## Parallel Execution & Reporting
- Use a task queue sized by `--threads` (default: hardware concurrency).
- Each task outputs structured log lines (JSON or plain text) summarizing:
  - File path.
  - Scenario name (if multi-scenario support introduced later).
  - Validation state.
  - Comparison result (pass/fail/disabled/updated).
- Aggregate exit code rules:
  - Non-zero if any file fails comparison or a golden is missing without suppression.
  - Zero if all enabled tests match (even when goldens updated via `--force-update`).
- Optional summary table at end showing totals.

## VS Code & ivgfiddle Workflow Hooks
- Extension can shell out to `IVGSnapshot --list-only` to populate scenario pickers.
- Provide task definitions for regenerating a specific IVG’s golden (`IVGSnapshot --force-update file.ivg`).
- Document how `validate:no` prevents CI noise while allowing designers to preview via ivgfiddle (which ignores `meta snapshot`).

## Implementation Roadmap
- [ ] Metadata discovery prototype: walk IVG sources, parse `meta snapshot` directives, and surface parsed key/value pairs via a lightweight API for early CLI plumbing tests.
- [ ] Inline ImpD evaluator bridge: execute the meta block’s ImpD payload in isolation, confirm variable injection into the main render context, and add regression coverage for parameter seeding.
- [ ] Golden lifecycle manager: codify `.png`, `.png.disabled`, and `--force-update` behaviors, including atomic rename/write operations and comprehensive logging of state transitions.
- [ ] Renderer integration: factor shared rasterization code from `ivg2png` into `libIVGSnapshot` and confirm identical output for sample fixtures.
- [ ] Comparison & diff suite: implement strict pixel comparison, diff artifact writing, and tolerance hooks guarded by configuration.
- [ ] Parallel executor: add job scheduling, progress reporting, and early-exit controls to handle large scenario batches efficiently.
- [ ] Developer tooling surface: wire VS Code tasks, ivgfiddle hooks, and documentation updates so the workflow from draft (`validate:no`) to sealed (`validate:yes`) is end-to-end demonstrable.

## Naming Brainstorm
- Harness alternatives: `IVGSnapshot`, `IVGRegression`, `IVGVerify`, `IVGGolden`, `IVGSeal`, `IVGGuardian`, `IVGCheck`,
  `IVGReferee`, `IVGCompare`, `IVGCanary`.
- Meta tag alternatives: `meta snapshot`, `meta proof`, `meta verify`, `meta seal`, `meta assure`, `meta expect`,
  `meta golden`, `meta assay`, `meta audit`, `meta check`.

