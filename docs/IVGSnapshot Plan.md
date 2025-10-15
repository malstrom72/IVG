# IVGSnapshot Implementation Plan (Revision 4)

## Review Response & Checklist
1. **Tool placement** – Every source file (implementation plus future helpers) lives directly under `tools/IVGSnapshot/`. No headers are exported to `tools/include/` or `src/`.
2. **Custom executor** – `IVGSnapshot` runs its own `IMPD::Interpreter` with a thin `IMPD::Executor` subclass whose only meaningful override is `meta(...)`.
3. **Argument parsing** – `IMPD::ArgumentsContainer::parse` (see `src/IMPD.h` lines 129-183) processes the raw `meta` arguments. We always call `throwIfAnyUnfetched()` before leaving `meta`.
4. **Version dispatch** – We match the normalized key `snapshot-1`. `IMPD::Interpreter` already appends the `-1` suffix when users type `meta snapshot` (see `src/IMPD.cpp` lines 478-501), so there is no manual fallback handling in the tool.
5. **Grammar** – The collector enforces `meta snapshot [validate:(yes|no)=yes] [scenario:<scenario>] [ list:[ [ <statements> ] [ <statements> ] ... ] | <statements> ]`.
   - `list:` provides an explicit list of bracketed statement blocks.
   - A single `<statements>` body may be bracketed or unbracketed (tokens after labels are concatenated with single spaces).
6. **Extensibility** – The new `list:` label replaces the previous implicit bracket-array form. The old comma-separated list is no longer accepted by the tool.
7. **Repeated scenarios & arrays** – Array entries (`[ [ do-stuff ], [ do-more ] ]`) generate numbered entries for a scenario. Repeating the same `scenario:` later merges into the existing scenario while appending new entries, as required by the example in the review comment.
8. **Default behavior** – There is no legacy fallback. When no `scenario:` label is supplied the collector synthesizes deterministic names (`<ivg-basename>-<block>` or `<ivg-basename>-<block>-<entry>`). The UI selects whichever entry the user asks for.
9. **Task queue** – Rendering uses NuXThreads (`externals/NuX/NuXThreads.*`). The scheduler allocates a bounded queue sized to a power of two, and a worker pool sized by `--threads` (defaulting to hardware concurrency).

## High-Level Architecture
- **Entry point** – `tools/IVGSnapshot/IVGSnapshot.cpp` owns `main`, command-line parsing, the metadata collector, render scheduler, and golden manager. No auxiliary headers are introduced.
- **Collector** – `SnapshotCollector` derives from `IMPD::Executor`. Aside from `meta`, every override simply returns success. The collector stores:
- Raw source text (for line tracking).
- Include search paths (re-used by `load`).
- A monotonic scan cursor used by `locateMetaLine` to find the next `meta snapshot` token.
- **Plan model** – `SnapshotPlan` groups entries by scenario. Each entry keeps an ordered list of `SnapshotInvocation` records (block ordinal, source line, statement ordinal, and captured statement string) so repeated `meta snapshot` directives append invocations to the same entry.
- **Renderer** – `SnapshotPlaybackExecutor` wraps `IVG::IVGExecutor` and replays stored statements ahead of each invocation before delegating to the regular IVG interpreter.
- **Scheduler** – Rendering currently runs serially through `renderPlan`; a later milestone will layer the NuXThreads-backed scheduler on top.
- **Golden manager** – `SnapshotGolden` will encapsulate PNG discovery, `.disabled` drafting, `.bak` backups, and diff generation.

## Collector Sketch (tabs preserved)
```cpp
// tools/IVGSnapshot/IVGSnapshot.cpp (lines ~40-210)
class SnapshotCollector : public IMPD::Executor {
public:
SnapshotCollector(SnapshotPlan& plan, const std::string& path, const IMPD::String& source,
const std::vector<std::string>& includeDirs);

bool meta(IMPD::Interpreter& interpreter, const IMPD::String& key,
const IMPD::String& arguments) override
{
if (key != "snapshot-1") {
return false;
}

IMPD::ArgumentsContainer args(IMPD::ArgumentsContainer::parse(interpreter, IMPD::StringRange(arguments)));

SnapshotBlock block;
block.validate = parseValidate(interpreter, args.fetchOptional("validate"));
const IMPD::String* scenarioLabel = args.fetchOptional("scenario");
if (scenarioLabel != 0) {
block.scenario = *scenarioLabel;
}

const IMPD::String* raw = args.fetchOptional(0, false);
if (raw == 0) {
IMPD::Interpreter::throwBadSyntax("snapshot meta requires a statement list.");
}

block.statements = parseStatements(interpreter, *raw);
block.sourceLine = locateMetaLine();
args.throwIfAnyUnfetched();

plan.addBlock(interpreter, block);
return true;
}

private:
IMPD::StringVector parseStatements(IMPD::Interpreter& interpreter, const IMPD::String& raw);
uint32_t locateMetaLine();
};
```

### Statement parsing details
- We rely on `ArgumentsContainer` to tokenize labels and positional arguments. After consuming `validate:`/`scenario:`/`list:`, any remaining positional tokens are concatenated with single spaces to form a single unbracketed body.
- For `list:`, we first call `Interpreter::expand()` on the labeled value to remove the outer brackets (while preserving nested blocks), then `Interpreter::parseList()` to extract each inner `[ ... ]` block as an element. Each element must be bracketed; we keep the inner content unchanged to preserve authoring whitespace.
- For a single bracketed body, we keep the inner content unchanged (no normalization). For an unbracketed body, we keep the concatenated string verbatim.

### Plan data model
```cpp
// tools/IVGSnapshot/IVGSnapshot.cpp (lines ~40-220)
struct SnapshotInvocation {
	uint32_t blockIndex;
	uint32_t sourceLine;
	uint32_t statementOrdinal;
	IMPD::String statements;
};

struct SnapshotEntry {
	uint32_t scenarioIndex;
	uint32_t entryOrdinal;
	bool validate;
	IMPD::String scenarioName;
	std::vector<SnapshotInvocation> invocations;
};

struct SnapshotScenario {
	IMPD::String name;
	bool validate;
	bool explicitScenario;
	std::vector<uint32_t> entryIndices;
	std::map<uint32_t, uint32_t> entryLookup;
};

class SnapshotPlan {
public:
	explicit SnapshotPlan(const std::string& ivgPath);
	void addBlock(IMPD::Interpreter& interpreter, const SnapshotBlock& block)
	{
		if (block.statements.empty()) {
			IMPD::Interpreter::throwBadSyntax("snapshot meta requires at least one statement block.");
		}

		const uint32_t blockOrdinal = nextBlockOrdinal;
		const bool hasExplicitScenario = !block.scenario.empty();
		if (hasExplicitScenario) {
			appendExplicitScenario(interpreter, blockOrdinal, block);
		} else {
			appendImplicitScenario(interpreter, blockOrdinal, block);
		}

		++nextBlockOrdinal;
	}

	const std::vector<SnapshotScenario>& getScenarios() const { return scenarios; }
	const std::vector<SnapshotEntry>& getEntries() const { return entries; }

private:
	void appendExplicitScenario(IMPD::Interpreter& interpreter, uint32_t blockOrdinal, const SnapshotBlock& block);
	void appendImplicitScenario(IMPD::Interpreter& interpreter, uint32_t blockOrdinal, const SnapshotBlock& block);
	uint32_t resolveScenario(IMPD::Interpreter& interpreter, const IMPD::String& name, bool validate, bool explicitScenario);
	SnapshotEntry& ensureEntry(uint32_t scenarioIndex, SnapshotScenario& scenario, uint32_t entryOrdinal, bool validate, const IMPD::String& scenarioName);
	IMPD::String synthesizeScenarioName(uint32_t blockOrdinal, uint32_t blockCount, uint32_t entryOrdinal) const;

	IMPD::String baseName;
	uint32_t nextBlockOrdinal;
	std::vector<SnapshotScenario> scenarios;
	std::vector<SnapshotEntry> entries;
	std::map<IMPD::String, uint32_t> scenarioLookup;
};
```

- `resolveScenario` rejects validation toggles across repeated `scenario:` blocks while remembering whether the scenario was explicitly named.
- `appendExplicitScenario` enforces a consistent number of entries when a named scenario appears multiple times, merging each invocation into its indexed entry.
- `appendImplicitScenario` synthesizes scenario names by combining the IVG basename with the block ordinal (and entry ordinal for arrays).
- Every entry maintains `invocations` so playback can re-run the same statement block at every collection point.

## Rendering & Scheduling Sketch
```cpp
// tools/IVGSnapshot/IVGSnapshot.cpp (lines ~340-520)
class SnapshotPlaybackExecutor : public IVG::IVGExecutor {
public:
	SnapshotPlaybackExecutor(const SnapshotPlan& plan, const SnapshotEntry& entry, uint32_t invocationIndex);
	bool meta(IMPD::Interpreter& interpreter, const IMPD::String& key, const IMPD::String& arguments) override;
	void onBeforeExecute(IMPD::Interpreter& interpreter, uint32_t blockIndex) override;

private:
	const SnapshotPlan& plan;
	const SnapshotEntry& entry;
	uint32_t invocationIndex;
	bool injected;
};

bool renderEntry(const SnapshotPlan& plan, const SnapshotScenario& scenario, const SnapshotEntry& entry, const SnapshotInvocation& invocation);
void renderPlan(const SnapshotPlan& plan);
```

- `SnapshotPlaybackExecutor` injects the stored statement list immediately before the matching block runs, falling back to the base executor for the remainder of the script.
- `renderEntry` loads the IVG, applies include/font/image paths, and sequentially instantiates `SnapshotPlaybackExecutor` for each invocation, verifying that statement ordinals remain monotonic.
- `renderPlan` iterates scenarios and entries; verbose mode prints include, font, and image directories before invoking `renderEntry`.
- Parallelisation is postponed: once NuXThreads scheduling lands, `renderPlan` will enqueue `renderEntry` calls on the worker pool instead of running them inline.

## Golden Lifecycle
- Goldens default to `<sanitized-source>__<scenario>.png` beside the IVG when `--snapshot-dir` is absent, where `sanitized-source` is the IVG path (without extension) relative to `--root-dir` (or the current working directory when omitted). If the IVG sits outside that root we fall back to the absolute path. Path separators map to a single `_` while existing underscores expand to `__` so nested IVGs produce unique flattened names.
- Draft mode (`validate:no`) writes `<scenario>.png.disabled` and records success without comparison.
- Validation mode requires an existing golden unless `--force-update` is supplied. When updating, the previous golden becomes `<scenario>.png.bak`.
- Compare failures drop `<scenario>.actual.png` and `<scenario>.diff.png` alongside the golden to help debugging.
- Reporting aggregates per-scenario timings, validation states, and failure reasons.

## Implementation Roadmap

### Milestone 1 – Metadata capture (complete)
- [x] Implement `SnapshotCollector`, `SnapshotPlan`, and supporting helpers inside `IVGSnapshot.cpp` using `IMPD::String` throughout.
- [x] Add focused tests in `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` that feed synthetic `meta snapshot` directives and assert plan contents (arrays, implicit names, repeated scenarios, validation conflicts).
- [x] Expose `--list-only` (already hooked up) and validate its textual output against known fixtures via `tools/IVGSnapshot/tests/ListOnlySample.{ivg,txt}`.
- [x] Run `timeout 600 ./build.sh`.

### Milestone 2 – Rendering execution (complete)
- [x] Load IVGs through a snapshot-local cached document helper and reuse the runtime renderer.
- [x] Inject each entry’s statement block before rendering; reuse `Interpreter::parseList` for bracket evaluation to avoid divergence.
- [x] Share include/font/image path handling with `ivg2png` (see `tools/ivg2png/IVG2PNG.cpp` lines 94-201).
- [x] Cache interpreter state across entries for the same IVG when possible to avoid redundant parsing.
- [x] Extend `--verbose` output to show resolved include paths, scenario names, and validation states.
- [x] Run `timeout 600 ./build.sh`.

### Milestone 3 – Golden lifecycle & reporting (in progress)
- [x] Implement `SnapshotGolden` with `.disabled`/`.bak` support and PNG comparison (leveraging `NuXPixels` diff helpers around `externals/NuX/NuXPixels.cpp:311-512`).
- [x] Define PNG search, draft promotion, and cleanup helpers that mirror the ivg2png workflow while remaining local to the tool.
- [x] Wrap NuXPixels diff entry points so failures report per-channel statistics and delta image paths.
- [x] Emit structured logs summarizing per-entry results and aggregate statistics.
- [x] Capture per-entry status (rendered, diffed, skipped) along with validation metadata for machine parsing.
- [x] Summarize counts and validation failures at the end of the run with clear exit codes.
- [ ] Add integration tests covering draft, validation, forced updates, and diff emission.
- [ ] Extend the snapshot fixtures to cover `.disabled` promotion, `.bak` creation, and diff outputs.
- [ ] Validate structured log output alongside PNG artifacts in the test harness.
- [x] Run `timeout 600 ./build.sh`.

### Milestone 4 – Parallel execution
- [x] Complete `SnapshotScheduler` on top of NuXThreads, including sentinel shutdown and `--exit-on-first-failure` support.
- [x] Ensure log output stays deterministic by tagging entries with `<ivg>#<scenario>#<block>#<entry>` identifiers.
- [x] Wire the renderer to enqueue jobs while respecting `--threads` and stop scheduling when a failure occurs and `--exit-on-first-failure` is set.
- [x] Export ivgfiddle manifests that list available scenarios and entries for tooling consumption.
- [x] Run `timeout 600 ./build.sh`.
