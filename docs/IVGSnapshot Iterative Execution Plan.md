IVGSnapshot: Iterative Single-Pass Execution Plan

## TODO Overview
- [x] Remove the separate “collect-then-playback” staging and run IVG files from the beginning each round, so variable state established by the renderer before a snapshot meta affects snapshot parsing and logic.
- [x] Execute the script end-to-end repeatedly (rounds). In each round, only the snapshots that match the round’s selected scenario + entry ordinal run; all other snapshots are observed but skipped. If any other snapshots are detected, schedule another round and repeat with the next selection. If no snapshots are encountered at all, render once and discard the image without treating the file as a failure.

## TODO Constraints
- [x] Preserve existing snapshot naming and golden file layout to avoid churn.
- [x] Continue supporting `validate:(yes|no)` and the new statement grammar:
        - [x] Single body via one positional argument
        - [x] Multiple bodies only via `list:[ [ ... ] [ ... ] ... ]`
- [x] Maintain `--list-only` output as a stable, deterministic list identical to previous semantics.
- [ ] Allow reduced per-file parallelism while keeping multi-file concurrency.

## TODO Terminology
- [x] Confirm scenario definitions via `scenario:<name>` or implicit generation.
- [x] Confirm entry ordinal as the 1-based index within each `list:` block (1 for single-body).
- [x] Confirm invocation meaning: one `meta snapshot` block occurrence in the source.

## TODO High-Level Design
- [x] Replace the plan-collection phase with an iterative runner that executes the source with a real `IVG::IVGExecutor` every time.
- [x] On the first encountered `meta snapshot` in a round, pin the round’s selection:
        - [x] Derive the pinned scenario from explicit `scenario:` or implicit naming (see Compatibility section).
        - [x] Derive the pinned entry ordinal from the statement ordinal within the `list:` (or 1 for single-body).
- [x] During the remainder of the round:
        - [x] Execute only snapshot invocations matching the pinned selection (same scenario and entry ordinal across subsequent blocks) and enforce consistent `validate:` flags.
        - [x] For other scenarios or entry ordinals, mark `moreRemaining = true` and skip them for the current round.
- [x] After the round finishes:
        - [x] If there was no pinned selection (no snapshots encountered), discard any image and mark file as successfully processed with 0 snapshots.
        - [x] If a snapshot was pinned, validate/write golden for that selection as today.
        - [x] If `moreRemaining` is set, schedule another round and select the next unseen snapshot in deterministic order (see Ordering).
- [x] Repeat rounds until no remaining snapshots are observed.

- [x] Maintain stable ordering to keep list-only output and golden filenames deterministic:
        - [x] Use first-seen scenario order based on source execution.
        - [x] Within each scenario, process entry ordinals in ascending order (1..N).
- [x] Maintain an ordered registry in memory per file during processing:
        - [x] Implement `struct SeenScenario { String name; bool explicitLabel; bool validate; uint32_t maxOrdinal; std::vector<bool> processed; }`.
        - [x] On observing a new scenario+ordinal, expand `maxOrdinal`/`processed`, pin if unprocessed and no current selection, otherwise mark `moreRemaining = true`.

## TODO Meta Semantics (Single-Runner)
- [x] Parse arguments using `ArgumentsContainer` on the live interpreter frame to respect variable-dependent expansions inside `list:`/blocks.
- [x] Apply statement parsing rules:
        - [x] When `list:` is present, expand the outer list (`expand()`), then `parseList()` into verbatim elements while keeping brackets and whitespace for each element.
        - [x] When no `list:` is present, fetch exactly one positional argument verbatim (`fetchRequired(0, false)`).
- [x] Apply selection rules:
        - [x] Let the first matching invocation pin `(scenario, entryOrdinal, validate)` for the round.
        - [x] For subsequent invocations:
                - [x] If same scenario and ordinal, parse statements, verify the selected element exists, and keep for ID generation/consistency.
                - [x] If same scenario but different ordinal, mark `moreRemaining = true` and skip.
                - [x] If different scenario, mark `moreRemaining = true` and skip.
- [x] Ensure scenario-wide `validate` flags remain consistent across blocks; report mismatches as round errors.

## TODO Compatibility: Scenario Names and Snapshot IDs
- [x] Preserve previous implicit naming to avoid golden churn:
        - [x] For single-body blocks without `scenario:`, use `implicit-<blockOrdinal>`.
        - [x] For `list:` blocks with K bodies without `scenario:`, use `implicit-<blockOrdinal>-<entryOrdinal>`.
- [x] Build entry identifiers as `<snapshotSourceTag>_<scenarioName>#<entryOrdinal>`.

## TODO CLI Behavior
- [x] Execute rounds without writing/validating images for `--list-only`. After the final round, print a merged listing of scenarios and entries in deterministic order with first-seen line numbers.
- [ ] Keep semantics for `--force-update`, `--threads`, `--verbose`, and `--exit-on-first-failure`, allowing per-file serial rounds but multi-file concurrency.

## TODO Error Handling
- [ ] If no snapshots are pinned in a round but the script throws, treat it as non-fatal: discard the image and continue to the next file.
- [ ] If a snapshot is pinned and rendering fails, record a failure for that specific entry.

## TODO Data Structures (New or Changed)
- [x] Define `SnapshotRoundState` (per round):
        - [x] Include `bool hasPinned; String scenario; uint32_t entryOrdinal; bool validate; uint32_t blockOrdinalCursor; bool moreRemaining;`.
- [x] Define `SnapshotProgress` (per file across all rounds):
        - [x] Maintain ordered `vector<SeenScenario>` and `map<String, size_t>` for lookup.
        - [x] Provide methods to choose the next unprocessed `(scenario, ordinal)` and to mark processed.

- [x] Delete the collection-only executor from IVGSnapshot:
        - [x] Remove `SnapshotCollector` and `SnapshotPlan` usage from the IVGSnapshot binary.
        - [x] Remove `TestSnapshotPlan` binary and associated tests; replace with new list-only fixtures.
- [x] Introduce `SnapshotRoundCoordinator` (or integrate into the main runner):
        - [x] Drive repeated execution rounds for a single file until all snapshots are processed.
        - [x] Maintain `SnapshotProgress` across rounds.
- [x] Refactor `SnapshotPlaybackExecutor` to support pin/skip logic:
        - [x] Add `SnapshotRoundState* round` pointer.
        - [x] In `meta(snapshot-1)`, call `parseSnapshotStatements()` on the live interpreter, then:
                - [x] If `!round->hasPinned`, pin `(scenario, ordinal, validate)` and record the first source line.
                - [x] Otherwise, compare; if not matching, set `round->moreRemaining = true` and return (skip).
                - [x] For matching invocations, verify the selected element exists and remains consistent across appearances.
- [x] Implement the rendering loop per file:
        - [x] Initialize a fresh `IVG::SelfContainedARGB32Canvas` and `SnapshotPlaybackExecutor` with the current round state for each iteration.
        - [x] Run the interpreter against the full source.
        - [x] If `!round->hasPinned`, discard the image and break.
        - [x] Otherwise, validate/write golden for the pinned selection as today.
        - [x] Record processed `(scenario, ordinal)` in `SnapshotProgress`.
        - [x] If `round->moreRemaining` or `SnapshotProgress` has another unprocessed entry, set the next target and loop again; otherwise break.
- [x] Update `--list-only` to run the same loop but never write/validate images, then print a synthesized plan from `SnapshotProgress` that mirrors current output (scenarios, entries, blocks/lines).
- [x] Keep shared caches (fonts/images) and PNG IO unchanged.
- [x] Preserve snapshot ID generation and directory layout, with `SnapshotGolden` receiving `(scenario, ordinal)` metadata on the fly instead of via a prebuilt plan.

## TODO Testing Plan
- [x] Replace `TestSnapshotPlan` with tests that:
        - [x] Verify `--list-only` output on existing fixtures matches the current expected text.
        - [x] Add a new fixture where snapshot `list:` contents depend on variables set before the meta to prove the single-runner model captures correct expansions.
        - [x] Validate that repeated blocks for the same scenario and ordinal are accepted while mismatched `validate:` flags are rejected.

## TODO Migration Notes
- [x] Remove or rewrite:
        - [x] `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp`
        - [x] Any references to `SnapshotPlan` in build scripts.
- [x] Add new `--list-only` expected outputs and ensure deterministic ordering.

## TODO Future Work (Optional)
- [ ] Persist a per-file snapshot index to allow resuming after partial runs.
- [ ] Allow targeting a specific scenario/ordinal via CLI (e.g., `--scenario <name> --entry <n>`).

## TODO Pseudocode Sketch

        RoundProgress progress;
        while (progress.hasNextTarget() || progress.empty()) {
                RoundState round = progress.makeRound();
                Canvas canvas;
                Executor exec(canvas, round, progress, options);
                Interpreter.run(source, exec);
                if (!round.hasPinned) break; // no snapshots in this file
                if (validate) golden.validateOrUpdate(canvas.raster, round);
                progress.markProcessed(round.scenario, round.entryOrdinal);
                if (!round.moreRemaining && !progress.hasNextTarget()) break;
        }

