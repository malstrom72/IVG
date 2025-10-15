# Streamlining `IVGSnapshot.cpp`

## Test coverage expansion plan

Before refactoring begins, add the following focused regression tests to protect current behaviour. Each new test suite should land alongside the production changes it guards, and every milestone ends with a successful `timeout 600 ./build.sh` run.

- [x] Snapshot plan fixtures that load multi-scenario IVGs and assert the emitted replay order, per-scenario frame counts, and error reporting to exercise `SnapshotPlan` simplifications. Implement as deterministic unit-style tests under `tests/` with golden JSON summaries for comparison.
- [x] Filesystem guard tests that simulate missing directories and permission failures, verifying the new `ensureDirectoryTree` helper throws and leaves existing artifacts untouched. Use temporary directories within the test harness.
- [x] Libpng IO tests that feed corrupted PNG streams and ensure RAII teardown does not leak handles while surfacing the same exceptions observed today. Cover both load and save paths.
- [x] Scheduler stress test that enqueues more jobs than worker slots and confirms ordering, cancellation, and shutdown semantics remain unchanged.
- [x] CLI parsing integration test that invokes `IVGSnapshot` with representative flag combinations, asserting exit codes and stderr messages for success, missing values, and unknown switches.

## Status review

All milestones in this document are now checked off. The scheduler runs with fixed worker slots, scenario failures are logged through the shared helper, and the CLI ladder uses the new value-extraction utility, so the simplification effort is complete.

This TODO list tracks concrete simplifications for the monolithic `tools/IVGSnapshot/IVGSnapshot.cpp`. Each milestone should conclude by running `timeout 600 ./build.sh` and verifying the `=== ALL BUILDS AND TESTS COMPLETED SUCCESSFULLY ===` footer.

- [x] Collapse filesystem helpers that wrap NuX calls in redundant sentinels, replacing them with a single throwing `ensureDirectoryTree` plus thin path adapters. Estimated reduction: ~45 lines.
- Example snippet:
```cpp
static void ensureDirectoryTree(const NuXFiles::Path &directory) {
if (directory.isNull() || directory.isRoot()) {
return;
}
std::vector<NuXFiles::Path> stack;
for (NuXFiles::Path cursor(directory); !cursor.exists(); cursor = cursor.getParent()) {
stack.push_back(cursor);
if (cursor.isRoot()) {
break;
}
}
for (std::vector<NuXFiles::Path>::reverse_iterator it = stack.rbegin(); it != stack.rend(); ++it) {
if (!it->tryToCreate() && !it->isDirectory()) {
throw NuXFiles::Exception(L"failed to create directory", 0);
}
}
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Make `SnapshotGolden` artifact handling data-driven by introducing `{path, role}` tables that share cleanup helpers across `writeDraft` and `validate`. Estimated reduction: ~50 lines.
- Example snippet:
```cpp
struct ArtifactPath { const char *role; const std::string *path; };
static void removeArtifacts(const ArtifactPath *artifacts, size_t count) {
for (size_t i = 0; i < count; ++i) {
removeFile(*artifacts[i].path);
}
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Wrap libpng usage with RAII handles so the PNG helpers focus on pixel conversion instead of manual `try`/`catch` cleanup. Estimated reduction: ~60 lines.
  - Completed via `ScopedFileHandle`, `ScopedPngReadStruct`, and `ScopedPngWriteStruct` wrappers with `loadPngRaster`/`writeRasterToPng` rewritten to use them.
- Example snippet:
```cpp
class ScopedFileHandle {
  public:
ScopedFileHandle(const std::string &path, const char *mode)
: file(std::fopen(path.c_str(), mode)) {}

~ScopedFileHandle() {
if (file != 0) {
std::fclose(file);
}
}

FILE *get() const { return file; }

  private:
FILE *file;
};

class ScopedPngReadHandle {
  public:
ScopedPngReadHandle() : png(0), info(0) {}

void initialize() {
png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0,
snapshotPNGError, 0);
if (png == 0) {
throw std::runtime_error("could not initialize PNG reader");
}
info = png_create_info_struct(png);
if (info == 0) {
throw std::runtime_error("could not initialize PNG info struct");
}
}

  private:
png_structp png;
png_infop info;
};
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Simplify `SnapshotPlan` storage by embedding entry vectors inside scenarios and removing duplicate maps, indices, and playback cursors. Estimated reduction: ~150 lines.
  - Completion note: Inlined entries into `SnapshotScenario`, removed the global entry table/lookups, and updated plan traversal utilities and tests.
- Example snippet:
```cpp
struct ScenarioPlan {
SnapshotScenario header;
std::vector<SnapshotEntry> entries;
};

SnapshotEntry &ensureEntry(ScenarioPlan &scenario, uint32_t ordinal) {
for (size_t i = 0; i < scenario.entries.size(); ++i) {
if (scenario.entries[i].entryOrdinal == ordinal) {
return scenario.entries[i];
}
}
scenario.entries.push_back(makeEntry(scenario.header, ordinal));
return scenario.entries.back();
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Share directory search logic among collectors, executors, font loaders, and image resolvers via a reusable helper that tries the working directory and then search paths. Estimated reduction: ~80 lines.
  - Completed via a templated `visitSearchPaths` helper reused by collectors, playback includes, font loading, and cached image resolution.
- Example snippet:
```cpp
template <typename Reader>
static bool visitSearchPaths(const std::string &localPath,
				const std::vector<std::string> &directories,
				const std::string &name,
				Reader &reader) {
	if (!localPath.empty() && reader(localPath)) {
		return true;
	}
	for (size_t i = 0; i < directories.size(); ++i) {
		const std::string candidate = directories[i] + "/" + name;
		if (reader(candidate)) {
			return true;
		}
	}
	return false;
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Replace manual mutex plumbing in `SharedResources` with `NuXThreads::Lockable` wrappers for font and image caches. Estimated reduction: ~30 lines.
- Completion note: `SnapshotPlaybackExecutor` now guards font and image caches via `NuXThreads::Lockable` locks, eliminating the duplicated mutex checks.
- Example snippet:
```cpp
struct SharedResources {
NuXThreads::Lockable<std::map<WideString, IVG::Font> > fonts;
NuXThreads::Lockable<std::map<std::string, CachedImage> > images;
};
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Slim `SnapshotScheduler` by moving to fixed worker slots guarded by a single mutex instead of custom job/result deques and sentinel tasks. Estimated reduction: ~90 lines.
- Completion note: Worker-local slots now track `hasJob`, `inProgress`, and `hasResult` flags under one mutex while threads wait on per-slot events, removing the old pending/result deques and sentinel jobs.
- Example snippet:
```cpp
struct WorkerSlot {
SnapshotJob job;
bool hasJob;
NuXThreads::Event ready;
};

bool SnapshotScheduler::enqueue(const SnapshotJob &job) {
NuXThreads::MutexLock lock(mutex);
if (!started || stopScheduling) {
return false;
}
slots[nextSlot].job = job;
slots[nextSlot].hasJob = true;
slots[nextSlot].ready.signal();
nextSlot = (nextSlot + 1) % slots.size();
return true;
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Centralize scenario failure logging with a helper so every return site emits the same `path: scenario name: message` pattern. Estimated reduction: ~20 lines.
- Completion note: `logScenarioFailure` now formats all scenario errors and fills in default text so `renderEntry` just records the outcome and returns.
- Example snippet:
```cpp
static void logScenarioFailure(const std::string &ivgPath,
const SnapshotEntryResult &result, const char *defaultMessage) {
const std::string &message = result.message.empty()
? defaultMessage : result.message;
std::cerr << ivgPath << ": scenario " << result.scenarioName
<< ": " << message << std::endl;
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [x] Tighten command line parsing with inline helpers for value extraction and unknown-flag reporting while keeping the existing ladder. Estimated reduction: ~25 lines.
- Completion note: `requireOptionValue` performs shared bounds checks for option values, trimming the repeated `i + 1 >= argc` branches while keeping the legacy ladder intact.
- Example snippet:
```cpp
static const char *requireOptionValue(const std::string &flag, int &i, int argc, char **argv) {
if (i + 1 >= argc) {
throw std::runtime_error(flag + " requires a value");
}
return argv[++i];
}
```
- Completion check: Run `timeout 600 ./build.sh`.
