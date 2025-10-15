# Snapshot Selection Popup Plan (Emscripten-first)

## Goals
* Detect `meta snapshot` directives in the currently edited IVG source by reusing the canonical `IVGSnapshot` implementation via the existing wasm toolchain.
* Surface a toolbar popup in ivgfiddle that lists every snapshot scenario/entry collected from the source so artists can pick which render to preview.
* Drive rasterization with the selected snapshot entry while delegating all parsing, validation, and playback semantics to the C++ snapshot runtime that already ships in IVG tools.
* Ensure the default render executes the full IVG using the runtime's snapshot scheduling so that scenario `#0` (or the first labeled entry) is drawn automatically before the UI exposes alternative scenarios.

## Snapshot runtime recap
* **Snapshot meta grammar** – `parseSnapshotStatements` now expects `list:` when a block contains multiple bodies, returning each list element verbatim while continuing to support a single positional argument for non-list blocks.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1747-L1767】
* **Scenario construction** – During collection the runtime resolves (and if needed synthesizes) scenario identifiers, guaranteeing stable indices and labels for every entry that will later be surfaced to the UI.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1469-L1673】
* **Scenario-gated playback** – Playback checks each `meta snapshot` invocation against the active scenario, enforces statement ordinals, and throws when the executed body diverges from what was collected, which lets the renderer run only the chosen scenario while still executing the entire IVG file.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L2004-L2046】

## Implementation outline

### 1. Capture snapshot plans during rasterization
* Extend the wasm rasterizer entry point so every render runs the interpreter through the entire IVG while attaching a `SnapshotCollector` to record the canonical plan alongside drawing.
* When the interpreter hits a snapshot block for the active scenario, execute the body immediately; if the block provides a `list:` argument, pick the first element (`#0`) for the default run and record the remaining entries as alternates.
* Cache the collected `SnapshotPlan` (scenarios and entry ordinals) so it can be returned with the rasterization result and reused for scenario switches without reparsing unless the source hash changes.

### 2. Scenario selection wiring in wasm
* Teach the wasm renderer to accept an optional selection descriptor (`scenarioId` or explicit scenario string plus list index) that is converted into the runtime's `scenarioIndex` / `entryOrdinal` pair before invoking `SnapshotPlaybackExecutor`.
* Default renders omit the descriptor, causing the runtime to pick the first available scenario using its own scheduling (`#0` or the first labeled entry). Subsequent renders pass the descriptor supplied by the UI so only matching snapshot blocks execute while the rest of the IVG continues to run normally.
* After each render, serialize the full scenario catalog (scenario names, entry ordinals, list labels) back to JS so the popup can show all alternatives that were parsed during the run.

### 3. JavaScript integration and popup UX
* Add a wasm bridge that invokes the rasterizer, receives both the image data and the serialized snapshot catalog, and memoizes the catalog against the current source hash.
* On initial render, pick the default entry from the returned catalog and highlight it in the toolbar; when the user chooses another scenario, resend the selection descriptor through the same rasterizer call.
* Update the popup UI to group entries by scenario name (using synthesized labels when necessary), annotate list entries with their index (e.g., `#2`) when a block exposed a `list:` argument, and persist the active selection through ivgfiddle's settings layer.

### 4. Testing & diagnostics
* Add C++ coverage that compiles under Emscripten to confirm the wasm renderer exports can round-trip a snapshot catalog and honor scenario descriptors during playback.
* Extend the ivgfiddle JS tests to load fixtures containing single-body and `list:` snapshot blocks, asserting that the default render uses the first entry and that alternative selections trigger the correct scenario/index pair.
* Keep logging high-value traces (`trace("Snapshot catalog: ...")`, `trace("Snapshot selection changed")`) to simplify debugging when source edits invalidate cached plans.

## Open questions / follow-ups
* Decide on a compact serialization format for the catalog returned from the wasm renderer (plain structs vs. JSON) that keeps round-trips cheap without reimplementing parsing in JS.
* Evaluate reusing the wasm snapshot bridge for VS Code preview or automation tooling once ivgfiddle integration is proven.
