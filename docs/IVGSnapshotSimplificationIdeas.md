# Streamlining `IVGSnapshot.cpp`

## Test coverage expansion plan

Before refactoring begins, add the following focused regression tests to protect current behaviour. Each new test suite should land alongside the production changes it guards, and every milestone ends with a successful `timeout 600 ./build.sh` run.

- [ ] Snapshot plan fixtures that load multi-scenario IVGs and assert the emitted replay order, per-scenario frame counts, and error reporting to exercise `SnapshotPlan` simplifications. Implement as deterministic unit-style tests under `tests/` with golden JSON summaries for comparison.
- [ ] Filesystem guard tests that simulate missing directories and permission failures, verifying the new `ensureDirectoryTree` helper throws and leaves existing artifacts untouched. Use temporary directories within the test harness.
- [ ] Libpng IO tests that feed corrupted PNG streams and ensure RAII teardown does not leak handles while surfacing the same exceptions observed today. Cover both load and save paths.
- [ ] Scheduler stress test that enqueues more jobs than worker slots and confirms ordering, cancellation, and shutdown semantics remain unchanged.
- [ ] CLI parsing integration test that invokes `IVGSnapshot` with representative flag combinations, asserting exit codes and stderr messages for success, missing values, and unknown switches.

This TODO list tracks concrete simplifications for the monolithic `tools/IVGSnapshot/IVGSnapshot.cpp`. Each milestone should conclude by running `timeout 600 ./build.sh` and verifying the `=== ALL BUILDS AND TESTS COMPLETED SUCCESSFULLY ===` footer.

- [ ] Collapse filesystem helpers that wrap NuX calls in redundant sentinels, replacing them with a single throwing `ensureDirectoryTree` plus thin path adapters. Estimated reduction: ~45 lines.
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

- [ ] Make `SnapshotGolden` artifact handling data-driven by introducing `{path, role}` tables that share cleanup helpers across `writeDraft` and `validate`. Estimated reduction: ~50 lines.
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

- [ ] Wrap libpng usage with RAII handles so the PNG helpers focus on pixel conversion instead of manual `try`/`catch` cleanup. Estimated reduction: ~60 lines.
- Example snippet:
```cpp
class PngHandle {
public:
PngHandle(png_structp s, png_infop i) : state(s), info(i) {}
~PngHandle() { png_destroy_read_struct(&state, &info, 0); }
png_structp state;
png_infop info;
};

class FileHandle {
public:
explicit FileHandle(const std::string &path, const char *mode)
: fp(std::fopen(path.c_str(), mode)) {
if (fp == 0) {
throw std::runtime_error("failed to open PNG file");
}
}
~FileHandle() { if (fp) { std::fclose(fp); } }
FILE *fp;
};
```
- Completion check: Run `timeout 600 ./build.sh`.

- [ ] Simplify `SnapshotPlan` storage by embedding entry vectors inside scenarios and removing duplicate maps, indices, and playback cursors. Estimated reduction: ~150 lines.
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

- [ ] Share directory search logic among collectors, executors, font loaders, and image resolvers via a reusable helper that tries the working directory and then search paths. Estimated reduction: ~80 lines.
- Example snippet:
```cpp
static bool readFromSearchPaths(const std::vector<std::string> &dirs,
const std::string &stem, String &contents) {
if (readFile(stem, contents)) {
return true;
}
for (size_t i = 0; i < dirs.size(); ++i) {
if (readFile(dirs[i] + "/" + stem, contents)) {
return true;
}
}
return false;
}
```
- Completion check: Run `timeout 600 ./build.sh`.

- [ ] Replace manual mutex plumbing in `SharedResources` with `NuXThreads::Lockable` wrappers for font and image caches. Estimated reduction: ~30 lines.
- Example snippet:
```cpp
struct SharedResources {
NuXThreads::Lockable<std::map<WideString, IVG::Font>> fonts;
NuXThreads::Lockable<std::map<std::string, CachedImage>> images;
};
```
- Completion check: Run `timeout 600 ./build.sh`.

- [ ] Slim `SnapshotScheduler` by moving to fixed worker slots guarded by a single mutex instead of custom job/result deques and sentinel tasks. Estimated reduction: ~90 lines.
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

- [ ] Centralize scenario failure logging with a helper so every return site emits the same `path: scenario name: message` pattern. Estimated reduction: ~20 lines.
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

- [ ] Tighten command line parsing with inline helpers for value extraction and unknown-flag reporting while keeping the existing ladder. Estimated reduction: ~25 lines.
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
