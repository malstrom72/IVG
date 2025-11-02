# TODO: `common:[ ... ]` Support for `meta snapshot`

- [x] **Reconfirm product goals before touching code**
  - `parseSnapshotBodies` threads the optional `common` argument alongside positional bodies, and `handleRoundMeta` executes the shared block before iterating the existing single or `list:[ ... ]` statements.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L2240-L2461】
  - Directives with only shared setup skip the scenario loop while remaining valid, and `SnapshotPlan::addBlock` persists such common-only blocks without fabricating plan entries.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L2423-L2482】【F:tools/ivgfiddle/src/rasterizeIVG.cpp†L600-L704】
  - Snapshot consumers that do not intercept `meta snapshot-1` still fall through (`meta` returns `false` for other keys), so no compatibility gate was required.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L2392-L2407】
  - Preview/catalog tooling continues to derive dropdown options solely from scenario entries while flagging shared-only catalogs, keeping selectors hidden when no entry bodies exist.【F:tools/ivgfiddle/src/rasterizeIVG.cpp†L1330-L1359】【F:tools/ivg-vscode/media/previewShared.js†L300-L462】

## TODOs for `tools/IVGSnapshot/IVGSnapshot.cpp`

- [x] **Extend argument parsing to capture `common` text verbatim**
  - `parseSnapshotStatements` currently returns only the scenario bodies (`tools/IVGSnapshot/IVGSnapshot.cpp:2222-2240`). Wrap or replace it with a helper that also calls `args.fetchOptional("common", false)` and preserves the raw text exactly like positional/list arguments.
  - Update `handleRoundMeta` (starting at `2355`) to call the new helper so the `common` payload and `StringVector` of scenario statements are available simultaneously.
  - Ensure `ArgumentsContainer::throwIfAnyUnfetched()` still succeeds by marking `common` as consumed and leaving existing validation paths untouched.

- [x] **Represent shared invocations in round state bookkeeping**
  - `SnapshotInvocation` / `SnapshotRoundState` are declared around `211-308` and currently track only `(blockIndex, entryOrdinal, statements)` for per-entry execution.
  - Introduce an explicit `InvocationKind` flag (e.g., enum with `Common` vs `Scenario`) or reserve `entryOrdinal == 0` for shared code; update `recordInvocation` and `findInvocation` to handle the extra variant while keeping the vector order stable for mismatches.
  - Thread the `sourceLine` associated with the `common` block so diagnostics continue to report accurate line numbers when round validation fails.

- [x] **Run and validate the `common` block every time `handleRoundMeta` executes**
  - Right after advancing `blockOrdinalCursor`, build a fresh `IVG::Context` (same pattern as lines `2411-2440` inside the existing loop) and invoke `runInNewContext` on the `common` statement before evaluating scenario entries.
  - Record the executed shared body in `SnapshotRoundState::recordInvocation` using the representation from the prior step; when revisiting the block on later rounds, reuse `round->findInvocation` to compare the `common` text for equality before running it.
  - Allow zero scenario statements when `common` is supplied by bypassing the `for` loop entirely but still applying selection validation: skip the `sawPinnedEntry` check if `statements.empty()` while preserving the "selected snapshot entry missing" guard for directives that do define per-scenario bodies.

- [x] **Keep list-only output and logging informative**
  - Inspect `SnapshotProgress::ensureDisplayLabel` and downstream logging that expects at least one entry (usage in `handleRoundMeta` lines `2376-2420`). Insert a conditional trace when `common` exists so verbose logging shows something like `block X common setup` without fabricating scenario labels.
  - Update `--list-only` reporting (`tools/IVGSnapshot/IVGSnapshot.cpp:2760-2805`) to mention the presence of a shared block alongside existing per-entry rows. Verify that JSON/catalog export (if any) remains unchanged except for optional metadata fields.

- [x] **Refresh documentation**
  - `docs/IVGSnapshot.md` lines `14-35` currently describe only single/list bodies. Add the `common:[ ... ]` grammar, call out that the shared body always runs, and provide a short example covering `common`-only directives.

## TODOs for `tools/ivgfiddle/src/rasterizeIVG.cpp`

- [x] **Add `common` storage to parsed metadata**
  - `ParsedSnapshotMeta` (lines `476-504`) and `SnapshotBlock` (lines `508-523`) need an optional `String common` member. Modify `parseSnapshotMetaArguments` to pull `args.fetchOptional("common", false)` and stash it before `args.throwIfAnyUnfetched()`.
  - Mirror the CLI helper: keep `common` text verbatim (no trimming) so the catalog and playback can round-trip the exact script.

- [x] **Relax plan admission rules for shared-only blocks**
  - `SnapshotPlan::addBlock` currently throws when `block.statements.empty()` (line `540`). Change the guard to allow empty statements when `block.common` is non-empty while still rejecting directives with neither `common` nor per-scenario bodies.
  - When registering explicit scenarios (lines `554-590`), skip pushing per-entry invocations if the directive only carries `common`. Maintain entry count validation by continuing to check sizes when scenario bodies exist.
  - For implicit scenarios (lines `591-611`), guard the synthesis path so `common`-only directives do not auto-create faux scenario names. Record the block ordinal for catalog completeness even when no entries are created.

- [x] **Persist shared invocations so playback and collection can execute them**
  - Extend `SnapshotInvocation` (lines `526-537`) with an `InvocationKind` flag or `bool isCommon`. When adding invocations inside `addBlock`, push one shared invocation per block (e.g., store `statementOrdinal = 0`) so later lookups can retrieve the `common` text.
  - Ensure `SnapshotEntry::invocations` ordering stays stable: shared invocation first (ordinal `0`) followed by per-entry statements so playback can run the shared code before scenario-specific bodies.

- [x] **Execute shared code in both collection and playback paths**
  - In `SnapshotExecutor::meta` (lines `924-1011`), inject `common` playback right after confirming the block targets the active scenario but before selecting `statementBody`. Pull the shared invocation from the plan (matching `blockIndex` + `InvocationKind::Common`) and execute it unconditionally.
  - Update `executeCollectionInvocation` (lines `1020-1036`) to invoke the stored shared invocation even when no per-scenario entries exist. Guard against double execution by checking the new flag/ordinal.
  - During playback validation, continue checking `blockValidate` consistency and statement equality for both shared and per-scenario bodies so mismatches raise the same errors as today.

- [x] **Keep catalog JSON and VS Code UI stable**
  - Added `SnapshotPlan::hasCommonBlocks()` / `hasCommonOnlyBlocks()` so `buildSnapshotCatalogJson` emits `hasCommon`/`hasCommonOnly` flags while continuing to skip shared-only directives in the `entries` arrays. 【F:tools/ivgfiddle/src/rasterizeIVG.cpp†L596-L671】【F:tools/ivgfiddle/src/rasterizeIVG.cpp†L1306-L1310】
  - Logged explicit catalog summaries in the VS Code preview when shared setup is present, ensuring the dropdown remains hidden for common-only plans while still advertising shared execution. 【F:tools/ivg-vscode/media/previewShared.js†L319-L387】【F:tools/ivg-vscode/media/previewShared.js†L409-L451】

## Testing Checklist

- [x] **CLI regression coverage**
  - Added `CommonBlock.ivg/.txt` to exercise shared-only and mixed directives under `--list-only`, verifying the new logging and entry totals. 【F:tools/IVGSnapshot/tests/CommonBlock.ivg†L1-L8】【F:tools/IVGSnapshot/tests/CommonBlock.txt†L1-L47】
  - Extended `TestSnapshotPlan.cpp` with targeted mismatch tests that reuse the new testing hook to surface `common` and per-scenario divergence errors across iterative executions. 【F:tools/IVGSnapshot/tests/TestSnapshotPlan.cpp†L78-L141】

- [x] **IVGFiddle validation**
  - Confirmed `rasterizeIVG` emits catalog metadata for common-only and mixed blocks by exercising the new fixture and inspecting the generated listing/logging, ensuring shared invocations execute before scenario playback. 【F:tools/ivgfiddle/src/rasterizeIVG.cpp†L560-L748】【F:tools/IVGSnapshot/tests/CommonBlock.txt†L1-L47】

- [x] **VS Code extension smoke tests**
  - Validated the preview controller consumes the new `hasCommon` flags and logs shared-only catalogs without surfacing empty selectors, mirroring manual expectations for common-only and mixed plans. 【F:tools/ivg-vscode/media/previewShared.js†L319-L451】

- [x] **Documentation review**
  - Revisited `docs/IVGSnapshot.md` to ensure the `common:[ ... ]` grammar and examples cover shared-only directives for both CLI and tooling users. 【F:docs/IVGSnapshot.md†L36-L76】

## Rollout Coordination

- [x] Publish IVGSnapshot CLI and VS Code updates together so catalog consumers never observe shared-only plans without matching playback logic.
- [x] Consider versioning the snapshot catalog schema if future UI work needs to distinguish shared blocks; leave a TODO comment pointing to the release where the field becomes required.
