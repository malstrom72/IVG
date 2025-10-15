# Snapshot Selection Popup Plan (Emscripten-first)

## Goals
* Detect `meta snapshot` directives in the currently edited IVG source by reusing the canonical `IVGSnapshot` implementation via the existing wasm toolchain.
* Surface a toolbar popup in ivgfiddle that lists every snapshot scenario/entry collected from the source so artists can pick which render to preview.
* Drive rasterization with the selected snapshot entry while delegating all parsing, validation, and playback semantics to the C++ snapshot runtime that already ships in IVG tools.

## Snapshot tooling recap
* **Collector / planning pipeline** – `SnapshotCollector`, `SnapshotPlan`, and `SnapshotPlaybackExecutor` already encapsulate discovery, naming, validation, and playback checks for `meta snapshot` directives. They expose all semantics we need (scenario merging, implicit names, validate flag propagation, entry ordinals).【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1452-L2052】
* **Multi-pass discovery** – The tool runs the interpreter repeatedly: `processFile` loops `beginCollection` → `run` → `completeCollectionPass` while `prepareNextCollectionPass` advances through the queued scenario/entry combinations. During the first pass, the default active scenario index (`0`) and entry ordinal (`1`) already target the first collected invocation, so the collector replays that scenario as soon as its `meta snapshot` block is parsed while leaving later scenarios for subsequent passes.【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1528-L1565】【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1709-L1753】【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1837-L1889】【F:tools/IVGSnapshot/IVGSnapshot.cpp†L2785-L2817】
* **Interpreter hooks** – The collector uses the interpreter `meta` callback to capture directives, and playback reuses the same callback to execute the chosen entry. Compiled for wasm, this logic can serve ivgfiddle identically to the command-line tool.
* **Existing wasm surface** – `tools/ivgfiddle/wasm/rasterizeIVG.cpp` already provides an exported entry point for rendering. We can extend it with additional exports that wrap the snapshot planner/collector rather than reimplementing parsing in JavaScript.【F:tools/ivgfiddle/wasm/rasterizeIVG.cpp†L1-L310】

## Implementation outline

### 1. Expose snapshot planning through the wasm module
* Create a thin C++ adapter inside `tools/ivgfiddle/wasm/` (e.g., `SnapshotExports.cpp`) that:
* Parses the incoming IVG source with the existing interpreter infrastructure.
* Runs a collecting pass with `SnapshotCollector`/`SnapshotPlan` to build the canonical plan.
* Serializes the result into a compact struct layout (array of scenarios, each with entries and metadata) that can be copied into wasm linear memory for consumption by JavaScript.
* Extend `rasterizeIVG.cpp` (or the new adapter) with `extern "C"` functions such as:
* `const SnapshotPlanExport* ivgfiddleCollectSnapshots(const char* utf8Source);`
* `RasterizeResult ivgfiddleRenderSnapshot(const RasterizeArgs* args, const SnapshotSelection* selection);`
* Ensure the exported plan includes:
* Stable scenario identifiers (scenario name or synthesized implicit names from `SnapshotPlan`).
* Entry ordinals and display labels (use `SnapshotPlan::formatScenarioName` helpers where available).
* Flags needed by the UI (e.g., `validate` state, source line for trace linking).
* Guard the new exports behind `#if defined(__EMSCRIPTEN__)` so native builds remain unaffected.

### 2. JavaScript bindings that lean on the wasm exports
* Add a helper module (e.g., `snapshotBridge.js`) that:
* Calls `Module._ivgfiddleCollectSnapshots` and decodes the returned struct using typed arrays/DataView without duplicating parsing logic.
* Converts the wasm-exported plan into plain JS objects (`{ scenarios: [...], entries: [...], collectionRuns: [...] }`) for the UI.
* Persists the latest binary buffer hash/signature alongside the parsed JS objects so recomputation happens only when the source changes.
* Update existing rasterization requests (`rasterizeIVG`) to forward the current snapshot selection through a new wasm call or extended options struct rather than switching behavior in JS.
* Provide a resilience path: if the wasm export returns null (e.g., syntax errors), fall back to baseline rendering and hide the popup.

### 3. Toolbar integration & popup UX (JS/HTML only)
* Reserve toolbar markup in `ivgfiddle.html` for the snapshot button + popup container, following the existing overlay patterns for focus management.【F:tools/ivgfiddle/ivgfiddle.html†L1-L400】
* In `ivgfiddle.js`:
* Subscribe to editor change events and invoke the wasm-backed `collectSnapshots` bridge when the document contains `meta snapshot` (quick substring precheck before invoking wasm to avoid unnecessary work).
* Store the resulting plan in state; when the plan is empty, hide/disable the toolbar button.
* Render the popup list grouped by scenario with entries inside each group, using the display labels supplied by the wasm plan.
* Persist the selected scenario/entry via `Settings` using a key that incorporates the plan hash.
* On selection change, rerun the preview by calling into the wasm rasterizer with the selection struct.
* Display the active selection in the toolbar button label, and close the popup on selection, Escape, or outside click.

### 4. Maintain snapshot-aware rasterization flow
* Modify the wasm rasterization entry point to accept optional snapshot selection data.
* When a selection is provided, instantiate `SnapshotPlaybackExecutor` with the stored plan and run only the chosen entry during rendering.
* Ensure the plan stays in sync between discovery and playback:
* Cache the serialized plan in JS and send a lightweight selection descriptor (`scenarioId`, `entryIndex`) for playback.
* On each render request, validate that the selection still exists in the latest plan; if not, default to the first entry and notify via `trace`.
* Make sure zoom rerenders and other automated reruns reuse the stored selection without re-collecting snapshots unless the source hash changes.

### 5. Testing & diagnostics
* Add C++ unit coverage for the new wasm exports (gated under `#ifdef __EMSCRIPTEN__` so they compile in native builds). Verify that `ivgfiddleCollectSnapshots` matches `SnapshotPlan::buildCollectionRuns` expectations using existing snapshot fixtures.【F:tests/TestSnapshotPlan.cpp†L1-L400】
* Extend the ivgfiddle JS test harness to:
* Load sample IVG snippets with multiple scenarios.
* Confirm that the bridge surfaces the correct plan data (scenario counts, implicit names, validate flags).
* Select alternate entries and assert that the wasm rasterizer acknowledges the requested scenario/ordinal (e.g., via a debug export or pixel comparison).
* Log useful traces (`trace("Snapshots discovered: ...")`, `trace("Snapshot selection reset")`) to aid manual debugging.
* Run `timeout 600 ./build.sh` to ensure the wasm build (and native builds) still pass once the new exports are introduced.

## Open questions / follow-ups
* Determine the most compact serialization format for the wasm bridge (plain structs vs. JSON via `EM_ASM`), balancing simplicity and download size.
* Decide whether to surface `validate` flags in the UI (e.g., visually tag `validate:no` entries) or just preserve them for potential future automation.
* Explore sharing the wasm snapshot bridge with other hosts (VS Code preview, automated documentation generators) once ivgfiddle integration proves stable.
