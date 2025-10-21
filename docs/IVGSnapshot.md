# IVGSnapshot

## Purpose and Scope
IVGSnapshot scans IVG sources for `meta snapshot` directives, replays the captured ImpD statements, and writes PNG goldens that can be compared in continuous integration. The tool covers two complementary needs:

- **Automated validation** – CI jobs render every validated scenario, compare the output against stored goldens, and fail the build when syntax errors or image diffs are detected. This keeps dynamic IVG content—including GUI layouts and procedural graphics—stable over time.
- **Interactive exploration** – Developers authoring IVGs that react to host-supplied variables can describe alternative scenarios directly in the source. IVGFiddle and the Visual Studio Code IVG extension read the same snapshot data so authors can pick a scenario from the preview’s drop-down and evaluate different code paths without editing the file.

The documentation targets contributors who maintain IVG sources, reviewers who inspect golden diffs, and tool developers integrating snapshot-aware previews.

## Source Layout and Build Artifacts
The executable lives in `tools/IVGSnapshot/IVGSnapshot.cpp`, a single translation unit that contains the collector, renderer, scheduler, and PNG diff helpers. It depends on the core IVG runtime (`src/IVG.cpp`), ImpD interpreter (`src/IMPD.cpp`), and NuX utilities for filesystem and threading support (`externals/NuX`). Building the repository with `./build.sh` or the platform-specific wrappers emits `output/IVGSnapshot` (or `output/IVGSnapshot.exe` on Windows). Regression coverage for the tool resides under `tools/IVGSnapshot/tests/`, which includes plan-parsing unit tests (`TestSnapshotPlan.cpp`), list-only fixtures, and a shell workflow that exercises draft and validation promotion.

## Snapshot Metadata Grammar
### `meta snapshot` directive
`SnapshotCollector::meta` reacts to the normalized key `snapshot-1`. Each directive accepts the optional keywords `validate:(yes|no)` and `scenario:<label>`, plus either a single statement body or `list:[ ... ]`:

- `validate:` defaults to `yes`. Draft entries (`validate:no`) render images without comparing against existing goldens. When you later flip the flag to `yes`, the validator requires a `.png.old` placeholder from the draft run, writes the freshly rendered image to `<stem>.png`, and reports that the draft was promoted. If no draft file exists the run fails with `missing golden` so authors know to regenerate the snapshot plan first.
- `scenario:` clusters entries by name. Repeating the same scenario later in the document appends additional entries while enforcing a consistent validation mode.
- Omitted `scenario:` labels cause the collector to synthesize a deterministic name from the IVG basename, block index, and entry ordinal so previews and CI runs can present distinct entries even when the author did not provide a custom name.

The collector throws syntax errors when directives omit statement payloads, mix validation settings for the same scenario, or leave unused arguments in the metadata.

### Statement Bodies and Lists
Authors can record either a single ImpD block (`meta snapshot [ set fill red ]`) or an explicit list (`meta snapshot list:[ [ set fill red ] [ set fill blue ] ]`). Each list element becomes its own entry while sharing the surrounding IVG setup. Statements are stored verbatim—including whitespace—so playback can inject the exact code that was captured during collection. During validation, the playback executor re-parses the source and ensures the targeted statement ordinal still matches the recorded body, throwing a syntax error if the IVG changed out from under the plan.

### Scenario Grouping and Entry Ordinals
Snapshot plans organize data as scenarios containing ordered entries. Every directive receives a monotonically increasing `blockIndex`, and each entry inside a scenario carries its own `entryOrdinal`. Implicit scenarios increment ordinals sequentially to preserve discovery order; explicit scenarios reuse their ordinals when additional directives append invocations to an existing entry. When jobs are scheduled, the tool composes stable identifiers using the sanitized snapshot base, scenario name, block index, and entry ordinal (for example `controls#dark-mode#2#1`).

## Naming and Output Paths
IVGSnapshot derives the output stem by normalizing the IVG path relative to `--root-dir` (default: the current working directory). Path separators collapse to single underscores while existing underscores are doubled to avoid collisions. For each entry the tool produces:

- `<stem>.png` – the canonical golden image when validation is enabled.
- `<stem>.png.old` – draft renders for entries that remain in `validate:no` or have not yet been promoted to validated goldens.
- `<stem>.actual.png` and `<stem>.diff.png` – side-channel artifacts that appear when comparisons fail, highlighting pixel deltas for review.
- `<stem>.png.bak` – the previous golden retained when `--force-update` promotes a new render.

When a run succeeds in validation mode the tool cleans up `.actual` and `.diff` files; failures keep them so reviewers can inspect the differences. Draft runs skip comparisons and leave the `.png.old` file in place until validation is enabled. When validation promotes a draft, IVGSnapshot writes a new golden from the current render and deletes the `.png.old` placeholder after confirming the promotion.

## Command-Line Interface
### Basic Usage
Invoke the executable with one or more IVG files:

```bash
./output/IVGSnapshot path/to/graphics.ivg
```

For each file, IVGSnapshot prints the discovered scenarios, renders every entry, and ends with a summary that reports totals, validation counts, and failure reasons. The process exits with a non-zero code whenever collection fails, an image diff occurs, or a validation error is raised.

### Options
| Option | Purpose | Notes |
| --- | --- | --- |
| `--include-dir <path>` | Adds an include search directory. | Directories are consulted before resolving relative `load` statements while collecting and rendering. |
| `--font-dir <path>` | Adds a font lookup directory. | Fonts are cached per path so repeated entries avoid reloading resources. |
| `--image-dir <path>` | Adds an external image directory. | Enables reusing PNG assets referenced from ImpD code without duplicating them in the IVG tree. |
| `--snapshot-dir <path>` | Overrides the default output directory. | By default goldens live next to each IVG; this option pushes everything under a central folder. |
| `--root-dir <path>` | Sets the root used when computing snapshot names. | Useful when CI runs inside a workspace checkout but goldens are stored elsewhere. |
| `--force-update` | Overwrites goldens even when diffs occur. | Keeps the prior golden as `<stem>.png.bak` so changes can be reviewed. |
| `--threads <n>` | Limits worker threads. | `0` chooses hardware concurrency, clamped to the number of entries so small plans do not oversubscribe. |
| `--list-only` | Prints the collected plan without rendering. | Mirrors the textual fixtures in `tools/IVGSnapshot/tests/ListOnlySample.*` and is ideal for code reviews. |
| `--verbose` | Emits detailed diagnostics. | Logs resolved include/font/image directories, per-entry identifiers, and progress markers. |
| `--exit-on-first-failure` | Stops scheduling additional jobs after the first error. | Helpful when CI should abort early to reduce noise. |
| `--help` | Prints the usage text. | Shows the same option table as above and exits. |

## Execution Flow
1. **Collection** – `SnapshotCollector` runs the IVG through the ImpD interpreter, intercepting `meta snapshot` directives to build a `SnapshotPlan`. This stage records source line numbers, statement ordinals, and scenario metadata.
2. **Scheduling** – The plan is fed into `SnapshotScheduler`, which prepares worker threads according to `--threads`. Jobs enqueue per entry, and the scheduler can stop early when `--exit-on-first-failure` is set.
3. **Rendering and comparison** – Each job replays the captured statements with `SnapshotPlaybackExecutor`, renders the IVG via the core runtime, and saves the PNG. Validation entries compare against existing goldens, computing per-channel diff statistics to drive the final report.
4. **Reporting** – After all jobs finish, the tool prints a summary that includes counters for total entries, validated entries, drafts, updates, and failures. The exit code mirrors the summary (`0` on success, non-zero on parse, render, or diff errors).

## Typical Workflows
### Draft → Validate Loop
Authors often start by marking new scenarios as drafts (`validate:no`). The first run generates `.png.old` placeholders. After reviewing the rendered output in IVGFiddle or the VS Code preview and confirming the behavior, authors flip the directive to `validate:yes`. The next IVGSnapshot run promotes the draft into an active golden, removes the `.png.old` staging file, and will fail future runs if the image changes unexpectedly. The workflow fixture `tools/IVGSnapshot/tests/WorkflowDraftValidate.sh` demonstrates the full lifecycle.

### Updating Goldens
When intentional visual changes occur, rerun IVGSnapshot with `--force-update`. The tool writes the new render on top of the golden, stashes the previous file as `.png.bak`, and keeps `.actual`/`.diff` artifacts so reviewers can confirm the update. This mode pairs well with code review diffs where image artifacts are attached alongside source changes.

### Scenario Review and Planning
`--list-only` outputs the parsed snapshot plan without touching the renderer. The textual report spells out scenarios, entry ordinals, and statement blocks, which is valuable when designing complex scenario trees or debugging why a preview presents unexpected entries. The fixtures `ListOnlySample.*` and `ListScenarioVariants.*` under `tools/IVGSnapshot/tests/` mirror the format so you can compare local output against known-good plans.

## Integration and Supporting Assets
- **Interactive tooling** – IVGFiddle and the VS Code IVG extension read snapshot metadata to populate a scenario chooser in their previews. Each scenario corresponds to a plan entry, so switching options in the UI mirrors what CI will validate.
- **Test suite** – `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` provides focused coverage for metadata parsing, ensuring the collector handles lists, implicit naming, validation enforcement, and repeated scenarios. Workflow scripts exercise end-to-end rendering behavior.
- **Design references** – The implementation background in `docs/IVGSnapshot Plan.md` and the metadata overview in `docs/IVG Snippet Test Concepts.md` describe the architectural decisions behind the current behavior and are useful when extending the tool.

## Troubleshooting
- **Missing resources** – Errors about fonts, images, or includes usually indicate the search paths do not cover required assets. Add the relevant `--font-dir`, `--image-dir`, or `--include-dir` and rerun.
- **Syntax mismatches** – If collection throws because statement bodies no longer match the stored plan, regenerate the plan with `--force-update` or adjust the IVG to restore parity before validating.
- **Diff analysis** – When validation fails, inspect the accompanying `.actual.png` and `.diff.png` files. The summary prints per-channel maximum and mean deltas to highlight the magnitude of change; large diffs often signal logic errors, while tiny deltas may stem from antialiasing or animation jitter.
- **Command-line errors** – The parser rejects unknown options or missing arguments and displays usage information. Ensure at least one IVG path is supplied.

## Future Enhancements / Open Questions
- Extend snapshot consumption to downstream tooling, such as automated changelog generators that embed updated goldens.
- Explore alternative output formats (e.g., WebP) for workflows where PNG diffs are too large.
- Investigate incremental diff review inside editors so authors can compare scenarios without leaving their IDE.
