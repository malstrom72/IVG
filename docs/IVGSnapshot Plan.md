# IVGSnapshot Implementation Plan (Revision 4)

## Review Response & Checklist
1. **Tool placement** – Every source file (implementation plus future helpers) lives directly under `tools/IVGSnapshot/`. No headers are exported to `tools/include/` or `src/`.
2. **Custom executor** – `IVGSnapshot` runs its own `IMPD::Interpreter` with a thin `IMPD::Executor` subclass whose only meaningful override is `meta(...)`.
3. **Argument parsing** – `IMPD::ArgumentsContainer::parse` (see `src/IMPD.h` lines 129-183) processes the raw `meta` arguments. We always call `throwIfAnyUnfetched()` before leaving `meta`.
4. **Version dispatch** – We match the normalized key `snapshot-1`. `IMPD::Interpreter` already appends the `-1` suffix when users type `meta snapshot` (see `src/IMPD.cpp` lines 478-501), so there is no manual fallback handling in the tool.
5. **Grammar** – The collector enforces `meta snapshot [validate:(yes|no)=yes] [scenario:<scenario>] [ <statements> ] | [ [ <statements> ], [<statements>], ... ]` exactly. Each `<statements>` block must be enclosed in square brackets, using `Interpreter::isBracketBlock` from `src/IMPD.cpp:779`.
6. **Extensibility** – No speculative array/map extensions are planned. Only the bracketed or bracket-array forms above are supported.
7. **Repeated scenarios & arrays** – Array entries (`[ [ do-stuff ], [ do-more ] ]`) generate numbered entries for a scenario. Repeating the same `scenario:` later merges into the existing scenario while appending new entries, as required by the example in the review comment.
8. **Default behavior** – There is no legacy fallback. When no `scenario:` label is supplied the collector synthesizes deterministic names (`<ivg-basename>-<block>` or `<ivg-basename>-<block>-<entry>`). The UI selects whichever entry the user asks for.
9. **Task queue** – Rendering uses NuXThreads (`externals/NuX/NuXThreads.*`). The scheduler allocates a bounded queue sized to a power of two, and a worker pool sized by `--threads` (defaulting to hardware concurrency).

## High-Level Architecture
- **Entry point** – `tools/IVGSnapshot/IVGSnapshot.cpp` owns `main`, command-line parsing, the metadata collector, render scheduler, and golden manager. No auxiliary headers are introduced.
- **Collector** – `SnapshotCollector` derives from `IMPD::Executor`. Aside from `meta`, every override simply returns success. The collector stores:
- Raw source text (for line tracking).
- Include search paths (re-used by `load`).
- A monotonic scan cursor used by `locateMetaLine` to find the next `meta snapshot` token.
- **Plan model** – `SnapshotPlan` owns:
- `SnapshotScenario` records (scenario name, validate flag, vector of entry indices).
- `SnapshotEntry` records (scenario index, block ordinal, entry ordinal, source line, validate flag, raw statements).
- A `std::map<IMPD::String, uint32_t>` so repeated scenarios collapse into a single plan entry.
- **Renderer** – A later milestone will reuse `IVG::Document` (see `src/IVG.cpp` lines 291-411) and `IVG::IVGExecutor` to replay the IVG with injected statements before rasterising.
- **Scheduler** – `SnapshotScheduler` wraps `NuXThreads::Queue<SnapshotJob>` and a vector of `NuXThreads::Thread`. Each worker repeatedly pops jobs, runs them, and stops at a sentinel job.
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
- The collector trims whitespace by walking the `IMPD::StringRange` (matching how `src/IVG.cpp` handles similar constructs around lines 1021 and 1416).
- Each entry uses `Interpreter::isBracketBlock` (`src/IMPD.cpp:779`) to assert the outer brackets.
- Array handling delegates to `Interpreter::parseList` so the syntax matches the runtime expression evaluator exactly.
- After stripping brackets we keep the raw statement body (no further formatting) to replay inside the renderer.

### Plan data model
```cpp
// tools/IVGSnapshot/IVGSnapshot.cpp (lines ~20-120)
struct SnapshotEntry {
uint32_t scenarioIndex;
uint32_t blockIndex;
uint32_t entryIndex;
uint32_t sourceLine;
bool validate;
IMPD::String scenarioName;
IMPD::String statements;
};

class SnapshotPlan {
public:
explicit SnapshotPlan(const std::string& path);
void addBlock(IMPD::Interpreter& interpreter, const SnapshotBlock& block)
{
if (block.statements.empty()) {
IMPD::Interpreter::throwBadSyntax("snapshot meta requires at least one statement block.");
}

const bool explicitScenario = !block.scenario.empty();
for (uint32_t i = 0; i < block.statements.size(); ++i) {
const uint32_t entryOrdinal = i + 1;
const IMPD::String scenarioName = (explicitScenario
? block.scenario
: synthesizeScenarioName(block.statements.size(), entryOrdinal));

const uint32_t scenarioIndex = resolveScenario(interpreter, scenarioName, block.validate);
appendEntry(scenarioIndex, entryOrdinal, block, i);
}

++nextBlockOrdinal;
}

private:
uint32_t resolveScenario(IMPD::Interpreter& interpreter, const IMPD::String& name, bool validate);
void appendEntry(uint32_t scenarioIndex, uint32_t entryOrdinal, const SnapshotBlock& block, size_t statementIndex);
IMPD::String synthesizeScenarioName(uint32_t blockCount, uint32_t entryOrdinal) const;
};
```

- `resolveScenario` ensures repeated `scenario:` labels keep the same validation mode (failing fast through `Interpreter::throwBadSyntax`).
- Implicit names combine the IVG basename with block and entry ordinals using `Interpreter::toString` (`src/IMPD.h:201-210`).

## Rendering & Scheduling Sketch
```cpp
// tools/IVGSnapshot/IVGSnapshot.cpp (later milestone)
class SnapshotJob {
public:
explicit SnapshotJob(SnapshotRenderer* renderer, const SnapshotEntry* entry);
void operator()() const;
static SnapshotJob MakeSentinel();
bool IsSentinel() const;
};

class SnapshotScheduler : private NuXThreads::Runnable {
public:
explicit SnapshotScheduler(uint32_t requestedThreads);
~SnapshotScheduler();

void Enqueue(const SnapshotJob& job);
void SignalShutdown();
void Join();

private:
void run() override;            // Matches NuXThreads::Runnable signature.

NuXThreads::Queue<SnapshotJob> queue;
std::vector<std::unique_ptr<NuXThreads::Thread>> threads;
std::atomic<bool> shuttingDown;
};
```

- Queue size: `nextPowerOfTwo(max<uint32_t>(requestedThreads, 1) * 4)` to satisfy `NuXThreads::Queue` alignment rules.
- `SignalShutdown` pushes one sentinel job per worker. Workers break out when `job.IsSentinel()` returns true.
- Command-line option `--threads` defaults to `NuXThreads::Thread::hardwareConcurrency()` (wrapper around `std::thread::hardware_concurrency()` inside the NuX wrapper).

## Golden Lifecycle
- Goldens default to `<basename>/<scenario>.png` beside the IVG when `--output-dir` is absent.
- Draft mode (`validate:no`) writes `<scenario>.png.disabled` and records success without comparison.
- Validation mode requires an existing golden unless `--force-update` is supplied. When updating, the previous golden becomes `<scenario>.png.bak`.
- Compare failures drop `<scenario>.actual.png` and `<scenario>.diff.png` alongside the golden to help debugging.
- Reporting aggregates per-scenario timings, validation states, and failure reasons.

## Implementation Roadmap

### Milestone 1 – Metadata capture (in progress)
- [x] Implement `SnapshotCollector`, `SnapshotPlan`, and supporting helpers inside `IVGSnapshot.cpp` using `IMPD::String` throughout.
- [ ] Add focused tests in `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` that feed synthetic `meta snapshot` directives and assert plan contents, including repeated-scenario collapsing and array handling.
- [ ] Expose `--list-only` (already hooked up) and validate its textual output against known fixtures.
- [ ] Run `timeout 600 ./build.sh`.

### Milestone 2 – Rendering execution
- [ ] Load IVGs via `IVG::Document` (`src/IVG.cpp:291-411`) and reuse the runtime renderer.
- [ ] Inject each entry’s statement block before rendering; reuse `Interpreter::parseList` for bracket evaluation to avoid divergence.
- [ ] Share include/font/image path handling with `ivg2png` (see `tools/ivg2png/IVG2PNG.cpp` lines 94-201).
- [ ] Cache interpreter state across entries for the same IVG when possible to avoid redundant parsing.
- [ ] Extend `--verbose` output to show resolved include paths, scenario names, and validation states.
- [ ] Run `timeout 600 ./build.sh`.

### Milestone 3 – Golden lifecycle & reporting
- [ ] Implement `SnapshotGolden` with `.disabled`/`.bak` support and PNG comparison (leveraging `NuXPixels` diff helpers around `externals/NuX/NuXPixels.cpp:311-512`).
- [ ] Emit structured logs summarizing per-entry results and aggregate statistics.
- [ ] Add integration tests covering draft, validation, forced updates, and diff emission.
- [ ] Run `timeout 600 ./build.sh`.

### Milestone 4 – Parallel execution
- [ ] Complete `SnapshotScheduler` on top of NuXThreads, including sentinel shutdown and `--exit-on-first-failure` support.
- [ ] Ensure log output stays deterministic by tagging entries with `<ivg>#<scenario>#<block>#<entry>` identifiers.
- [ ] Wire the renderer to enqueue jobs while respecting `--threads` and stop scheduling when a failure occurs and `--exit-on-first-failure` is set.
- [ ] Export ivgfiddle manifests that list available scenarios and entries for tooling consumption.
- [ ] Run `timeout 600 ./build.sh`.
