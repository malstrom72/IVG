IVGSnapshot: Iterative Single-Pass Execution Plan

Overview
- Goal: Remove the separate “collect-then-playback” staging and run IVG files from the beginning each round, so variable state established by the renderer before a snapshot meta affects snapshot parsing and logic.
- Model: Execute the script end-to-end repeatedly (rounds). In each round, only the snapshots that match the round’s selected scenario + entry ordinal run; all other snapshots are observed but skipped. If any other snapshots are detected, schedule another round and repeat with the next selection. If no snapshots are encountered at all, render once and discard the image without treating the file as a failure.

Constraints
- Preserve existing snapshot naming and golden file layout to avoid churn.
- Continue supporting `validate:(yes|no)` and the new statement grammar:
	- Single body via one positional argument
	- Multiple bodies only via `list:[ [ ... ] [ ... ] ... ]`
- `--list-only` still prints a stable, deterministic list identical to previous output semantics.
- Parallelism may be reduced per-file (rounds are inherently serial), but multi-file concurrency can remain.

Terminology
- Scenario: The label provided by `scenario:<name>` or an implicit generated name.
- Entry ordinal: The 1-based index of the selected element within a `list:` block; for single-body blocks it is always 1.
- Invocation: One `meta snapshot` block occurrence in the source.

High-Level Design
1. Replace the plan-collection phase with an iterative runner that executes the source with a real `IVG::IVGExecutor` every time.
2. On the first encountered `meta snapshot` in a round, pin the round’s selection:
	- Pinned scenario = explicit `scenario:` if present, otherwise an implicit scenario name derived from block ordinal and statement ordinal (see Compatibility section).
	- Pinned entry ordinal = the statement ordinal within the `list:` (or 1 for single-body).
3. During the remainder of the round:
	- Execute only snapshot invocations that match the pinned selection (same scenario and same entry ordinal across all subsequent blocks). Enforce consistent `validate:` flags.
	- For snapshot invocations of other scenarios or other entry ordinals, mark `moreRemaining = true` and skip them for this round.
4. After the round finishes:
	- If there was no pinned selection (no snapshots encountered), discard any image and mark file as successfully processed with 0 snapshots.
	- If a snapshot was pinned, validate/write golden for that selection as today.
	- If `moreRemaining` is set, schedule another round. Next round selects the next unseen snapshot in deterministic order (see Ordering).
5. Repeat rounds until no more remaining snapshots are observed.

Ordering and Determinism
- Ordering must be stable to keep list-only output and golden filenames deterministic:
	- First dimension: first-seen scenario order (based on first encounter in source during execution).
	- Second dimension: ascending entry ordinal (1..N) within that scenario.
- Implementation: Maintain an ordered registry in memory per file during the overall processing session:
	- `struct SeenScenario { String name; bool explicitLabel; bool validate; uint32_t maxOrdinal; std::vector<bool> processed; }`
	- When a new scenario+ordinal is observed, expand `maxOrdinal` and `processed` as needed. If the entry is unprocessed and no selection is pinned for the current round, pin it. Otherwise set `moreRemaining = true`.

Meta Semantics (Single-Runner)
- Parse arguments using `ArgumentsContainer` on the live interpreter frame (no dry-run), so
	variable-dependent expansions inside `list:`/blocks reflect real state.
- Statement parsing rules:
	- `list:` present → expand outer list (`expand()`), `parseList()` into verbatim elements (keep brackets and whitespace of each element).
	- No `list:` → fetch exactly one positional argument verbatim (`fetchRequired(0, false)`).
- Selection rules:
	- First matching invocation pins `(scenario, entryOrdinal, validate)` for the round.
	- Subsequent invocations:
		- If same scenario and same ordinal → mandatory: parse statements and verify that the selected element exists; keep them for ID generation and consistency checks.
		- If same scenario but different ordinal → mark `moreRemaining = true`; skip.
		- If different scenario → mark `moreRemaining = true`; skip.
- Validation flag:
	- The scenario-wide `validate` must remain consistent across all blocks belonging to the same scenario. Mismatch → error for that round.

Compatibility: Scenario Names and Snapshot IDs
- Preserve the previous implicit naming to avoid golden churn:
	- For a single-body block without `scenario:`, implicit name: `implicit-<blockOrdinal>`.
	- For a `list:` with K bodies without `scenario:`, implicit names: `implicit-<blockOrdinal>-<entryOrdinal>`.
- Build entry identifier as before: `<snapshotSourceTag>_<scenarioName>#<entryOrdinal>`.

CLI Behavior
- `--list-only`: Execute rounds without writing/validating images. Print after the final round a merged listing of all scenarios and entries in deterministic order, including line numbers from the first-seen occurrences.
- `--force-update`, `--threads`, `--verbose`, `--exit-on-first-failure`: unchanged semantics; per-file rounds remain serial, but files can still be processed across worker threads.

Error Handling
- If no snapshots are pinned in a round but the script throws, treat as non-fatal and continue (discard image and continue to next file).
- If a snapshot is pinned and rendering fails, record a failure for that specific entry.

Data Structures (New or Changed)
- `SnapshotRoundState` (per round):
	- `bool hasPinned; String scenario; uint32_t entryOrdinal; bool validate; uint32_t blockOrdinalCursor; bool moreRemaining;`
- `SnapshotProgress` (per file across all rounds):
	- Ordered `vector<SeenScenario>` and `map<String, size_t>` for lookup.
	- Methods to choose next unprocessed `(scenario, ordinal)` and to mark processed.

Implementation Steps
1. Delete the collection-only executor from IVGSnapshot:
	- Remove `SnapshotCollector` and `SnapshotPlan` usage from the IVGSnapshot binary.
	- Remove `TestSnapshotPlan` binary and associated tests; replace with new list-only fixtures.
2. Introduce `SnapshotRoundCoordinator` (or integrate into the main runner):
	- Drives repeated execution rounds for a single file until all snapshots are processed.
	- Maintains `SnapshotProgress` across rounds.
3. Refactor `SnapshotPlaybackExecutor` to support pin/skip logic:
	- Add `SnapshotRoundState* round` pointer.
	- In `meta(snapshot-1)`, call `parseSnapshotStatements()` on the live interpreter, then:
		- If `!round->hasPinned`, pin `(scenario, ordinal, validate)` and record first source line.
		- Else, compare; if not matching, set `round->moreRemaining = true` and return (skip).
		- For matching invocations, verify selected element exists and is consistent across appearances.
4. Rendering loop per file:
	- While (true):
		- Initialize a fresh `IVG::SelfContainedARGB32Canvas` and `SnapshotPlaybackExecutor` with the current round state.
		- Run the interpreter against the full source.
		- If `!round->hasPinned`: discard image; break.
		- Else validate/write golden for the pinned selection as today.
		- Record processed `(scenario, ordinal)` in `SnapshotProgress`.
		- If `round->moreRemaining` or `SnapshotProgress` has another unprocessed entry → set next target and loop again; otherwise break.
5. Update `--list-only` to run the same loop but never write/validate images. At the end, print a synthesized plan from `SnapshotProgress` resembling current output (scenarios, entries, blocks/lines).
6. Keep shared caches (fonts/images) and PNG IO as-is.
7. Preserve snapshot ID generation and directory layout; `SnapshotGolden` remains responsible for golden paths and diffs; it now receives on-the-fly `(scenario, ordinal)` metadata instead of looking it up from a prebuilt plan.

Testing Plan
- Replace `TestSnapshotPlan` with tests that:
	- Verify `--list-only` output on existing fixtures matches current expected text.
	- Add a new fixture where snapshot `list:` contents depend on variables set before the meta to prove the single-runner model captures correct expansions.
	- Validate that repeated blocks for the same scenario and ordinal are accepted and mismatched `validate:` flags are rejected.

Migration Notes
- Remove or rewrite:
	- `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp`
	- Any references to `SnapshotPlan` in build scripts.
- Add new `--list-only` expected outputs; ensure deterministic ordering.

Future Work (Optional)
- Persist a per-file snapshot index to allow resuming after partial runs.
- Allow targeting a specific scenario/ordinal via CLI (e.g., `--scenario <name> --entry <n>`).

Pseudocode Sketch

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

