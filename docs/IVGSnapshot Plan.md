# IVGSnapshot System Plan

## Goals
- Build a standalone C++ snapshot runner (`IVGSnapshot`) that renders `.ivg` sources, compares the output against golden PNGs, and reports per-scenario verdicts.
- Let authors opt into validation directly inside IVG documents through the `meta snapshot` directive without touching the runtime renderer.
- Keep every line of implementation isolated under `tools/IVGSnapshot/` so the core runtime in `src/` stays unchanged and no shared `tools/include` tree is introduced.

## Source Layout & Dependencies
- Implement the entire tool inside `tools/IVGSnapshot/IVGSnapshot.cpp` so helper types stay local to the binary without introducing scattered headers or auxiliary translation units.
- Reuse existing libraries by including headers from `src/` or `externals/` directly; do not add helper include directories outside the tool’s folder.
- The build target publishes an executable only; shared libraries stay in their existing locations.

## Command-Line Interface
`IVGSnapshot [options] <ivg> [<ivg> ...]`

Options:
- `--include-dir <path>` (repeatable) — forwarded to the IVG interpreter for `include` resolution.
- `--font-dir <path>` (repeatable) — additional font lookup paths.
- `--image-dir <path>` (repeatable) — bitmap lookup paths.
- `--output-dir <path>` — overrides the golden and diff root; defaults to the IVG directory.
- `--force-update` — replaces goldens with new renders (unless the block is `validate:no`).
- `--threads <n>` — limits concurrent work items; defaults to hardware concurrency.
- `--list-only` — parses metadata and prints the run list without rendering.
- `--verbose` — expands logging with resolved arguments, resolved golden locations, and NuXThreads scheduling diagnostics.
- `--exit-on-first-failure` — stops scheduling new renders once a failure is detected.

## Metadata Grammar & Collector Flow
- Grammar (first revision only):
  `meta snapshot [validate:(yes|no)=yes] [scenario:<scenario>] [ <statements> ] | [ [ <statements> ], [<statements>], ... ]`
  - Either supply a single block `[ <statements> ]` or an array of statement lists.
  - The directive may appear multiple times within one IVG. Execution order follows document order.
- `snapshot-1` is the only supported key. ImpD already appends `-1` when the author omits it, so the collector simply compares the normalized key and ignores everything else.
- The collector runs a dedicated `IMPD::Interpreter` instance with a thin executor that only cares about metadata. The executor never leaks outside `tools/IVGSnapshot/`.
- `IMPD::ArgumentsContainer::parse` is used to normalize arguments. The collector must call `throwIfAnyUnfetched()` so stray labels are rejected early.
- Line tracking: load the IVG text into memory, run the interpreter, and maintain a moving pointer through the source. Each time `meta` fires, advance through the buffer to the next `meta snapshot` token and count newlines to derive a 1-based line number for diagnostics.

### Collector Sketch
```cpp
// tools/IVGSnapshot/SnapshotCollector.cpp
class SnapshotCollector : public IMPD::Executor {
public:
	SnapshotCollector(SnapshotPlan& plan, IVG::Document& document)
	: plan(plan)
	, document(document)
	, scanOffset(0)
	{
	}

	bool meta(IMPD::Interpreter& interpreter, const IMPD::String& key, const IMPD::String& arguments) override
	{
		if (key != "snapshot-1") {
			return false;        // Unrecognized meta, fall through to other handlers.
		}

		IMPD::ArgumentsContainer args(IMPD::ArgumentsContainer::parse(interpreter, IMPD::StringRange(arguments)));
		SnapshotBlock block;
		block.validate = parseValidate(args.fetchOptional("validate"));
		const IMPD::String* scenarioLabel = args.fetchOptional("scenario");
		if (scenarioLabel != 0) block.scenario.assign(scenarioLabel->begin(), scenarioLabel->end());

		block.statements = extractStatementLists(arguments);
		block.sourceLine = locateMetaLine(document.getSource(), scanOffset, arguments);

		args.throwIfAnyUnfetched();
		plan.addBlock(block);
		return true;
	}

private:
	static ValidateMode parseValidate(const IMPD::String* value);
	static StatementList extractStatementLists(const IMPD::String& arguments);
	static uint32_t locateMetaLine(const std::string& source, size_t& scanOffset, const IMPD::String& arguments);

	SnapshotPlan& plan;
	IVG::Document& document;
	size_t scanOffset;
};
```
- `extractStatementLists` handles both the single-block form and nested arrays by re-using `IMPD::Interpreter::parseList` so the syntax matches the runtime exactly.
- `locateMetaLine` advances a local scan offset through the IVG source, matching the next `meta snapshot` occurrence and counting newline characters to produce deterministic line numbers.

## Snapshot Data Model & Scenario Rules
```cpp
// tools/IVGSnapshot/SnapshotPlan.h
struct SnapshotEntry {
	uint32_t blockIndex;
	uint32_t entryIndex;
	uint32_t sourceLine;
	std::string scenario;          // Empty -> synthesized name
	std::string statements;        // Raw ImpD snippet to replay
	ValidateMode validate;
};

struct SnapshotScenario {
	std::string name;              // Explicit scenario or synthesized ordinal
	std::vector<SnapshotEntry> entries;
	bool validate;
};

class SnapshotPlan {
public:
	void addBlock(const SnapshotBlock& block)
	{
		SnapshotScenario& scenario = resolveScenario(block);
		for (size_t i = 0; i < block.statements.size(); ++i) {
			SnapshotEntry entry;
			entry.blockIndex = blockOrdinal++;
			entry.entryIndex = static_cast<uint32_t>(i + 1);
			entry.sourceLine = block.sourceLine;
			entry.scenario = scenario.name;
			entry.statements = block.statements[i];
			entry.validate = block.validate;
			scenario.entries.push_back(entry);
		}
	}

	const std::vector<SnapshotScenario>& getScenarios() const { return scenarios; }

private:
	SnapshotScenario& resolveScenario(const SnapshotBlock& block);

	std::vector<SnapshotScenario> scenarios;
	uint32_t blockOrdinal = 1;
};
```
- Repeated `scenario:` labels reuse the same `SnapshotScenario` so all checkpoints contribute to a single golden even when separated by unrelated code.
- When no scenario is supplied, the planner synthesizes names using `<basename>-<blockOrdinal>` for single-entry blocks and `<basename>-<blockOrdinal>-<entryIndex>` for array entries.
- The first block’s first entry is the default selection for ivgfiddle or `--list-only`, not because of legacy compatibility but because we need a deterministic anchor when the user does not specify anything else.

## Execution Semantics
- Each snapshot entry replays the entire IVG with the collected statements inserted before rendering, matching how runtime playback works today.
- Array forms (`[ [ do-stuff ], [ do-other-stuff ] ]`) create multiple entries for the same block. They iterate sequentially in the order provided. When combined with `scenario:<name>`, all entries populate a single golden.
- Multiple `meta snapshot` blocks with the same `scenario` append additional statement groups; the renderer runs them all but only writes one golden per scenario.
- There is no future-looking parameter sweep system in this revision—only the grammar above is supported.

## Rendering & Resource Resolution
- Reuse `IVG::IVGExecutor` to evaluate the document with each snapshot entry’s statements, relying on the existing renderer in `src/IVG.cpp`.
- Share asset resolution (includes, fonts, images) with `ivg2png`, respecting the CLI search path arguments.
- Cache glyph and bitmap resources within a run so repeated entries for the same IVG avoid reloading files.

## Golden Lifecycle
- Golden names default to the IVG basename plus scenario or ordinal suffix; store them beside the IVG unless `--output-dir` redirects to a mirrored folder.
- Draft mode (`validate:no`) records a `.png.disabled` sentinel containing the freshly rendered output and skips comparisons.
- Validation mode requires an existing golden unless `--force-update` is active, in which case the new render replaces the file after optionally backing up the previous golden as `<name>.png.bak`.
- Every compare failure emits `.actual.png` and `.diff.png` artifacts next to the golden for debugging.

## Parallel Execution with NuXThreads
- Workers are implemented by subclassing `NuXThreads::Thread` so we stay inside the provided abstraction. Jobs are queued through `NuXThreads::Queue`.
- The scheduler owns a bounded queue sized to a power of two (per NuXThreads requirements) and a pool of worker threads sized by `--threads` (or hardware concurrency when omitted).
- Each job carries a stable identifier (filename + scenario + entry) so logs can correlate to the correct entry regardless of execution order.

### Scheduler Sketch
```cpp
// tools/IVGSnapshot/SnapshotScheduler.cpp
class SnapshotScheduler : private NuXThreads::Runnable {
public:
	explicit SnapshotScheduler(uint32_t requestedThreads)
	: threadCount(requestedThreads ? requestedThreads : defaultHardwareConcurrency())
	, queue(nextPowerOfTwo(threadCount * 4))
	{
		workers.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i) {
			workers.emplace_back(new NuXThreads::Thread(*this));
			workers.back()->start();
		}
	}

	void enqueue(SnapshotJob job)
	{
		while (!queue.push(job)) {
			NuXThreads::Thread::yield();
		}
	}

	void join()
	{
		queue.push(SnapshotJob::makeSentinel());
		for (size_t i = 0; i < workers.size(); ++i) {
			workers[i]->join();
		}
	}

	void run() override
	{
		SnapshotJob job;
		while (queue.pop(job)) {
			if (job.isSentinel()) break;
			job();
		}
	}

private:
	uint32_t threadCount;
	NuXThreads::Queue<SnapshotJob> queue;
	std::vector<std::unique_ptr<NuXThreads::Thread>> workers;
};
```
- The queue sentinel shuts down each worker cleanly when `join()` is called.
- `SnapshotJob` is a move-only functor that captures the renderer, plan entry, and reporting sink.

## Reporting
- Each completed job records: IVG path, scenario name, entry ordinal, validation mode, comparison outcome, golden path, and timing data.
- `--verbose` prints per-entry traces immediately; default output summarizes passes, failures, disabled entries, and updates at the end.
- Exit code is non-zero if any validating entry fails or a required golden is missing.

## Implementation Roadmap

### Milestone 1 — Metadata Capture
- [ ] Wire a new `SnapshotCollector` that subclasses `IMPD::Executor`, implements `meta`, and ignores all other callbacks.
- [ ] Implement `locateMetaLine` by scanning the raw IVG source and counting newlines up to each `meta snapshot` hit.
- [ ] Model `SnapshotBlock`, `SnapshotScenario`, and `SnapshotPlan`; include deterministic scenario merging for repeated labels.
- [ ] Add unit coverage in `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` that feeds synthetic argument strings through the collector and verifies the resulting plan.
- [ ] Expose a CLI flag (`--list-only`) that prints the collected plan so the behavior can be inspected without rendering.
- [ ] Run `timeout 600 ./build.sh` and ensure it completes successfully.

### Milestone 2 — Rendering Loop & Statement Injection
- [ ] Reuse `IVG::Document` loading logic so the same parser feeds both collector and renderer.
- [ ] Implement an evaluator that replays the document while injecting each entry’s statements before rendering.
- [ ] Share include/font/image search path handling with `ivg2png` to avoid duplicating resource code.
- [ ] Cache interpreter contexts between entries from the same IVG when possible to avoid redundant setup.
- [ ] Extend the plan listing to show synthesized scenario names and resolved statement counts for diagnostics.
- [ ] Run `timeout 600 ./build.sh` and confirm success.

### Milestone 3 — Golden Lifecycle & Comparison
- [ ] Implement `SnapshotGolden` to manage `.png`, `.png.disabled`, and `.png.bak` transitions according to the validation mode and `--force-update` flag.
- [ ] Implement pixel comparison with diff artifact writers (actual, diff, summary stats).
- [ ] Provide clear error messages when a validating entry lacks a golden and `--force-update` is not present.
- [ ] Add integration tests that cover draft mode, golden creation, forced updates, and diff emission.
- [ ] Produce structured log output (JSON or plain text) that downstream tooling can parse.
- [ ] Run `timeout 600 ./build.sh` and confirm success.

### Milestone 4 — Parallel Execution & Reporting Polish
- [ ] Implement `SnapshotScheduler` on top of `NuXThreads::Thread` and `NuXThreads::Queue`, including graceful shutdown and early-exit support.
- [ ] Add the `--threads` flag with hardware-concurrency default detection.
- [ ] Ensure log output remains deterministic when jobs finish out of order by tagging entries with stable identifiers.
- [ ] Integrate `--exit-on-first-failure` so the scheduler stops queueing new jobs after a failing comparison.
- [ ] Document ivgfiddle integration, including manifest export listing scenarios and entries with their indices.
- [ ] Run `timeout 600 ./build.sh` and confirm success.
