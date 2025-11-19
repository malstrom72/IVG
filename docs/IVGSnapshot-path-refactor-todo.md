# TODO: Replace raw path strings in IVGSnapshot with `NuXFiles::Path`

## Context and goals
- `tools/IVGSnapshot/IVGSnapshot.cpp` mixes UTF-8 strings and ad-hoc helpers (`joinPath`, `extractDirectory`, etc.) to describe filesystem objects across caching, CLI parsing, snapshot emission, and diff management.
- NuX already exposes a rich `NuXFiles::Path` API (see `externals/NuX/NuXFiles.h`) with platform-aware operations, lazy normalization, and helper classes for reading/writing files.
- The refactor should migrate IVGSnapshot to store and pass `NuXFiles::Path` objects wherever paths are manipulated, limiting raw string usage to user-facing I/O (CLI arguments, log output, JSON serialization).

## Checklist

- [x] **Step 1 – Understand the existing NuX filesystem API surface**
  - Verified constructors, null-state handling, and relative input support for `NuXFiles::Path`, including the ability to wrap platform-specific implementations when needed.
  - Captured comparison and sorting semantics via `compare`, `equals`, and relational operators to preserve deterministic ordering.
  - Documented relative navigation helpers (`getParent`, `getRelative`, `makeRelative`, `withoutExtension`, `withExtension`, `listSubPaths`, `matchesFilter`) for replacing current string concatenation.
  - Noted component extraction (`hasExtension`, `getName`, `getExtension`, `getNameWithExtension`, `getFullPath`) and filesystem queries (`exists`, `isFile`, `isDirectory`, `getInfo`, `updateAttributes`, `updateTimes`).
  - Summarized shell-style operations (`create`, `tryToCreate`, `copy`, `moveRename`, `erase`, `tryToErase`, `createTempFile`) that will replace bespoke directory/file management helpers.
  - Recorded related file-access primitives (`ReadOnlyFile`, `ReadWriteFile`, `ExchangingFile`) for snapshot read/write paths currently built on strings.

- [x] **Step 2 – Inventory every string-backed path in IVGSnapshot**
  - Catalogued every struct/class that still persists paths as UTF-8 strings: `CommandLineOptions` keeps directory lists (`includeDirs`, `fontDirs`, `imageDirs`), the optional `snapshotDir`, and raw `ivgPaths`; `SnapshotEntryResult` mirrors per-entry filesystem artifacts (`ivgPath`, `goldenPath`, `oldPath`, `actualPath`, `diffPath`, `backupPath`); `SnapshotRunResult` exposes `fileError` text that currently embeds native paths; and `SnapshotGolden` caches all golden/artifact stems as strings produced by `initializePaths`.
  - Traced how CLI parsing (`parseCommandLine`) seeds those members, how expansion helpers (`directoryExists`, `collectDirectoryIvgFiles`, `expandInputPaths`) treat them, and where they flow: playback (`SnapshotPlaybackExecutor::resolveRelativePath`, `loadFromDirectories`, image/font search), diffing (`hasAnyGoldensForSource`, `buildSnapshotSourceTag`), and filesystem mutation helpers (`ensureDirectory`, `ensureParentDirectory`, `removeFileIfExists`, `renameFile`).
  - Identified intentional string hold-outs that must remain (or gain string mirrors): sanitization utilities (`sanitizeFileComponent`, `buildSnapshotSourceTag`), human-readable output (`printScenarioListing`, status/log formatting, `formatErrorMessage`), and caches keyed by raw strings (`SharedResources::images`, `SnapshotProgress` identifiers) that currently rely on native path text.

- [x] **Step 3 – Introduce canonical path conversion utilities**
  - Plan to consolidate the ad-hoc helpers near `pathFromNativeString` into a dedicated bridge (e.g., `struct SnapshotPathBridge`) that owns: UTF-8-to-`NuXFiles::Path` conversion for CLI arguments, a `std::string toNative(const NuXFiles::Path &)` shim (wrapping `getFullPath()` + `pathStringFromWide`), and explicit fallbacks for null paths so logging still works.
  - The bridge will expose join/relative helpers built on `NuXFiles::Path` (`append`, `makeRelative`, `withExtension`) to replace manual concatenation in `initializePaths`, `resolveRelativePath`, `loadFromDirectories`, and wildcard assembly (`hasAnyGoldensForSource`, `collectOrphanGoldens`).
  - Call sites that need strings (filesystem APIs lacking NuX overloads, JSON/logging) will request string views from the bridge rather than rebuilding conversions, centralizing normalization and reducing duplicated error handling.

- [x] **Step 4 – Refactor core data structures to store `NuXFiles::Path`**
  - **Command-line inputs**
    - [x] Store `NuXFiles::Path` in `CommandLineOptions::includeDirs`, `fontDirs`, and `imageDirs`, keeping bridge-based conversions for CLI echoes and playback resolution.
    - [x] Persist the optional snapshot directory as `NuXFiles::Path` and retain a display mirror for user-facing text.
    - [x] Convert `CommandLineOptions::ivgPaths` to `NuXFiles::Path`, updating `expandInputPaths`, `collectDirectoryIvgFiles`, and usage sites.
      - `expandInputPaths` and `collectDirectoryIvgFiles` now operate on `Path` and sort by canonical native path.
      - CLI and reporting now stringify via `SnapshotPathBridge::toNative` at call sites; no parallel raw strings are stored.
    - [x] Ensure CLI error/reporting strings pull from bridge-generated native text instead of storing parallel raw strings.
  - **Playback and caches**
    - [x] Update `SharedResources` caches (`images`, `fonts`) to key off `NuXFiles::Path` (or normalized string mirrors) instead of raw input strings.
      - Image cache now normalizes keys via `SnapshotPathBridge` to canonical native paths, avoiding duplicate entries for the same file referenced through different relative forms.
      - Font cache already keys by font name (`WideString`) rather than raw file path; no change needed for this pass.
    - [x] Convert any remaining playback helpers that accept string paths (e.g., font/image resolution) to take `NuXFiles::Path` and stringify only when invoking legacy APIs.
      - `SnapshotPlaybackExecutor::loadImageFromPath` now accepts `NuXFiles::Path` and resolves a canonical native path internally; callers pass `Path` candidates.
  - **Per-entry artifacts**
    - [x] Have `SnapshotGolden` cache golden/draft/actual/diff/backup paths as `NuXFiles::Path`, stringifying only at filesystem/reporting boundaries.
    - [x] Store `NuXFiles::Path` inside `SnapshotEntryResult` for all per-entry artifacts, limiting bridge conversions to logging and serialization.
    - [x] Thread `NuXFiles::Path` through `SnapshotRunResult` (including failure context) so runtime bookkeeping no longer persists native strings for filesystem objects.
      - Added `filePath` field to `SnapshotRunResult` and set it from the processed IVG path; reporting continues to derive display text via the bridge.
    - [x] Audit any per-entry metadata that still persists native strings (e.g., diff status, orphan audits) and convert to `NuXFiles::Path` with bridge-based display helpers.
      - All per-entry filesystem artifacts in `SnapshotEntryResult` are `Path`. Remaining strings are human-readable (`scenarioName`, `message`, `identifier`) and intentionally remain text.

- [ ] **Step 5 – Update filesystem helpers and execution paths**
  - [ ] Add `Path` overloads for core helpers
    - [x] `fileExists(const NuXFiles::Path&)` alongside the existing string version. Callers: tests use string; IVGSnapshot internals can move to `Path`.
      - Current string helper: `tools/IVGSnapshot/IVGSnapshot.cpp:1228`
    - [x] `directoryExists(const NuXFiles::Path&)` in parallel with string variant.
      - Current string helper: `tools/IVGSnapshot/IVGSnapshot.cpp:1239`
    - [x] `ensureDirectory(const NuXFiles::Path&)` and `ensureParentDirectory(const NuXFiles::Path&)` that delegate to `ensureDirectoryPath`.
      - Existing string wrappers: `tools/IVGSnapshot/IVGSnapshot.cpp:1368`, `tools/IVGSnapshot/IVGSnapshot.cpp:1375`
    - [x] `removeFileIfExists(const NuXFiles::Path&)` with identical semantics (ignore errors) and keep string overload for tests.
      - Existing string impl: `tools/IVGSnapshot/IVGSnapshot.cpp:1386`
    - [x] `renameFile(const NuXFiles::Path&, const NuXFiles::Path&, std::string& error)`; keep string wrapper for external callers/tests.
      - Existing string impl: `tools/IVGSnapshot/IVGSnapshot.cpp:1457`

  - [x] Convert golden workflow to use `Path` helpers end-to-end
    - [x] `SnapshotGolden::writeDraft`/`validate`/diff paths: pass `NuXFiles::Path` directly to `ensureParentDirectory`, `removeFileIfExists`, and `renameFile` instead of converting to native strings at each call site.
      - Replaced all remove/rename/ensure calls in `writeDraft` and `validate` to call Path overloads; IO calls still stringify once per operation.
      - Current string usages to replace: `tools/IVGSnapshot/IVGSnapshot.cpp:1826-1833, 1855-1857, 1877, 1890-1894, 1901, 1907, 1914-1915, 1921, 1937-1940, 1954-1956, 1994-1995, 2129-2130, 2151-2152`.
    - [ ] Keep IO boundaries string-based for now (`writeRasterToPng`, `loadPngRaster`) but centralize the `SnapshotPathBridge::toNative` conversion in a single local variable per call to avoid scattered stringification.
      - IO functions: `tools/IVGSnapshot/IVGSnapshot.cpp:1789`, `tools/IVGSnapshot/IVGSnapshot.cpp:2358`

  - [x] Thread `Path` through snapshot root and golden discovery
    - [x] Change `resolveSnapshotRoot` to accept `const NuXFiles::Path&` and return `NuXFiles::Path` without string conversions (added overload; kept string version for callers pending migration).
      - Current signature: `tools/IVGSnapshot/IVGSnapshot.cpp:1044`
    - [x] Change `hasAnyGoldensForSource` to accept `const NuXFiles::Path& ivgPath` and `const std::string& snapshotBase`, using the new `resolveSnapshotRoot` overload (added overload; kept string version).
      - Current signature and call: `tools/IVGSnapshot/IVGSnapshot.cpp:1066`, used at `tools/IVGSnapshot/IVGSnapshot.cpp:3223`
    - [x] Update `processFileIterative` to pass the `Path` form (`run.filePath`) to these helpers and only stringify for logging.

  - [x] Migrate orphan golden audit to `Path` where appropriate
    - [x] Build `auditRoots` as `std::set<NuXFiles::Path>` instead of strings and adjust `collectOrphanGoldens` to accept `Path` roots (added overload).
      - Current string-based construction: `tools/IVGSnapshot/IVGSnapshot.cpp:3669-3699`
    - [x] In `collectOrphanGoldens`, receive `const std::set<NuXFiles::Path>& auditRoots` and iterate using `getFullPath()`; still return orphan list as native strings for human-readable diagnostics.
      - Current function: `tools/IVGSnapshot/IVGSnapshot.cpp:1396`
    - [ ] Keep `processedBases` as strings (they’re sanitized identifiers, not filesystem paths).

  - [x] Reduce ad-hoc stringification within reporting
    - [x] Add overloads: `abbreviatePathForDisplay(const NuXFiles::Path&)` and `printPathDetail(label, const NuXFiles::Path&)` to remove repeated bridge calls.
      - Added `abbreviatePathForDisplay(const NuXFiles::Path&)` which forwards to the string overload.
      - Current string-centric helpers: `tools/IVGSnapshot/IVGSnapshot.cpp:1479`, `tools/IVGSnapshot/IVGSnapshot.cpp:1586`
    - [x] Update call sites in reporting (`printEntryReport`) to use the `Path` overload directly.

  - [x] Keep compatibility shims during transition
    - [x] Retain string overloads for helpers used by tests (`TestSnapshotPlan.cpp`) and any scripts; mark them as adapters around the new `Path` implementations.
      - Tests use: `fileExists`, `ensureDirectory`, `removeFileIfExists`, `ParentDirectory(...)` wrappers.
    - [ ] Once all internal call sites are updated to `Path`, consider deprecating the string overloads with comments and migrate tests in Step 7.

  - [x] Sanity pass and compile-only validation
    - [x] After each conversion cluster (helpers, golden workflow, audit), rebuild IVGSnapshot to ensure no missing overloads and that logging still stringifies via the bridge.
      - Build script: `tools/IVGSnapshot/build-ivgsnapshot.sh`

- [x] **Step 6 – Adapt reporting, CLI parsing, and caching to the new types**
  - [x] Wrap CLI argument ingestion in bridge helpers that immediately materialize `NuXFiles::Path` objects, retaining native strings only for diagnostics and compatibility with existing fixture text.
  - [x] Adjust reporting utilities (`printScenarioListing`, `printEntryReport`, `formatErrorMessage`, `buildSnapshotSourceTag`, audit logs) to accept `Path` inputs and use a single conversion utility when they need human-readable output.
  - [x] Audit any remaining caches keyed by string (sanitized identifiers, scenario labels) and document which ones must stay as strings after the core path migration.

- [x] **Step 7 – Verify behavior and adjust tests**
  - [x] Rebuilt with `bash tools/IVGSnapshot/build-ivgsnapshot.sh` and focused test binary via `tools/BuildCpp.sh … output/TestSnapshotPlan`.
  - [x] Ran `./output/TestSnapshotPlan` (requires elevated permissions in sandbox due to /var/tmp usage); all tests passed without fixture changes.
  - [x] No updates needed to `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` or golden text fixtures; bridge-produced strings match expectations.

- [x] **Step 8 – Retire legacy helper functions**
  - [x] Removed unused string overloads where safe:
    - Deleted `collectOrphanGoldens` overload that accepted string roots.
    - Deleted `hasAnyGoldensForSource` overload that accepted string IVG path. Kept Path overload.
  - [x] Kept necessary string shims for tests and existing APIs:
    - Retained `resolveSnapshotRoot(const std::string&, …)` (used by `SnapshotGolden::initializePaths`).
    - Retained `buildSnapshotSourceTag(const std::string&, …)` (used by tests) and added a `Path` overload used by runtime.
  - [x] Verified remaining string concatenation only exists in `SnapshotPathBridge` or intentional reporting code.

- [x] **Step 9 – Clarify inline documentation**
  - [x] Added class-level comments:
    - `CommandLineOptions`: uses `NuXFiles::Path` for all paths; retains `snapshotDirDisplay` for user-facing echoes.
    - `SnapshotPathBridge`: centralizes conversions; path math done via `Path`; stringification only at IO/report boundaries.
    - `SnapshotGolden`: caches artifact paths as `Path`; stringifies only for IO and messages.
    - `SnapshotPlaybackExecutor`: explains Path-based resource resolution and string fallback.
    - `SnapshotRunResult`: documents `filePath` and why `fileError` etc. remain strings.
  - [x] Documented helper section: Path overloads preferred; string adapters retained for tests/compat.
  - [x] Rebuilt to verify comments don’t impact compilation.

- [ ] **Step 10 – Update developer-facing references**
  - Revise `docs/IVGSnapshot.md` (and related developer notes) to describe the new path workflow, bridge helpers, and focused build commands once the refactor is merged.
  - Document any migration guidance for contributors (e.g., converting custom scripts or tests) that previously relied on string-based helpers that will be removed in Step 8.

## Risk mitigation and open questions
- Confirm NuX path behavior on Windows vs. POSIX to avoid breaking CLI inputs that previously relied on raw string normalization.
- Decide whether caches keyed by path should use `Path::equals` semantics or string normalization to avoid duplicate loads when the same file is referenced via different relative paths.
- Ensure error messages still surface human-readable paths (convert via `getFullPath()` or relative representation) and preserve existing wording to keep fixtures stable.
 - [ ] **Step 11 – Eliminate remaining string-based path parameters**
  - Core workflow (process + reporting)
    - [x] Add `processFile(const CommandLineOptions&, const NuXFiles::Path&)` and migrate all internal call sites from the string overload; keep string wrapper only for tests/compat.
    - [x] Add `logFileReport(const NuXFiles::Path&, const SnapshotRunResult&, const CommandLineOptions&)` overload and migrate callers; keep string wrapper for legacy format tests.
    - [x] Change `printScenarioListing` to support Path via shim and migrate call site(s); stringify via `abbreviatePathForDisplay(path)`.

  - Golden path initialization
    - [x] Change `SnapshotGolden` ctor to accept `const NuXFiles::Path& ivgPath` (snapshotBase/scenario remain strings) and thread through to `initializePaths`.
    - [x] Change `initializePaths` signature to add a Path overload and switch usage to `resolveSnapshotRoot(Path, ...)`.
    - [x] Update call sites to pass the Path form (`run.filePath`).

  - Playback (source + includes/images/fonts)
    - [x] Add a `SnapshotPlaybackExecutor` ctor overload that accepts `const NuXFiles::Path& sourcePath` and sets both `sourcePathObj` and the display string; migrate `processFileIterative` to pass Path.
    - [x] Change `resolveRelativePath` to return `NuXFiles::Path` (added `resolveRelativePathPath` and migrated internal uses in image/font resolution); legacy string form retained for fallback.
    - [x] Add `readFile(const NuXFiles::Path&, IMPD::String&)` and migrate playback (`load`, `loadFromDirectories`) to prefer the Path overload with string fallback.
    - [x] Ensure `loadImageFromPath` already takes `Path` and that all callers pass `Path` candidates where possible.

  - IO primitives
    - [x] Add overloads: `loadPngRaster(const NuXFiles::Path&, ...)` and `writeRasterToPng(const NuXFiles::Path&, ...)` that locally stringify once; migrate internal calls in `SnapshotGolden` to these overloads.
    - [x] Keep string overloads for tests and external scripts; start removing duplicated ad-hoc `toNative(...)` temporaries at call sites (in progress).

  - Reporting helpers
    - [x] Add `logFileReport(const NuXFiles::Path&, const SnapshotRunResult&)` (compat signature) and migrate internal uses; keep existing string-only overload for older callers.
    - [x] Sweep `formatErrorMessage` and other utilities to accept `Path` where appropriate; only stringify for final display lines (abbreviation/printPathDetail already Path-aware).

  - Helper cleanups and removals
    - [x] Sweep for remaining uses of string helpers in production code; converted to Path variants in golden workflow and image resolution; retained string adapters for tests.
    - [x] Deprecate and comment `resolveSnapshotRoot(const std::string&, ...)` after `SnapshotGolden` migration; removal planned once no call sites remain.
    - [x] Deprecate and comment `buildSnapshotSourceTag(const std::string&, ...)` with Path overload preferred; string overload retained for tests.

  - Verification
    - [x] Rebuild IVGSnapshot and `TestSnapshotPlan` after each conversion cluster; keep fixture text stable by centralizing stringification in bridge/reporting shims.

- [ ] **Step 12 – Migrate tests to Path-only APIs**
  - Test scaffolding helpers (convert to Path)
    - [ ] Change `RunListOnlyTool` to accept `const NuXFiles::Path&` and call `processFile(options, path)` and `logFileReport(path, run)` using Path overloads.
      - File: `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp`
    - [ ] Replace `JoinPath(const std::string&, const std::string&) -> std::string` with `JoinPathPath(const NuXFiles::Path&, const std::string&) -> NuXFiles::Path` using `SnapshotPathBridge::append`.
      - Update all call sites to use the Path-returning helper.
    - [ ] Add `ParentDirectory(const NuXFiles::Path&) -> NuXFiles::Path` using `getParent()` and remove the string-based `ParentDirectory(const std::string&)`.
    - [ ] Add `WriteTemporaryIVGPath(const std::string& contents) -> NuXFiles::Path` (create native temp filename, then `SnapshotPathBridge::fromNative`); use when creating temporary `.ivg`/`.png` test files.
    - [ ] Provide `OpenForWrite(const NuXFiles::Path&) -> std::ofstream` by stringifying once via `SnapshotPathBridge::toNative(path)` for standard IO only.

  - Replace string-based calls with Path overloads
    - [ ] Replace all `processFile(options, std::string)` calls with `processFile(options, NuXFiles::Path)`.
    - [ ] Replace all `logFileReport(std::string, ...)` calls with `logFileReport(NuXFiles::Path, ...)`.
    - [ ] Replace `buildSnapshotSourceTag(std::string, rootPath)` with `buildSnapshotSourceTag(NuXFiles::Path, rootPath)`.
    - [ ] Replace `ensureDirectory(std::string)` with `ensureDirectory(NuXFiles::Path)`.
    - [ ] Replace `removeFileIfExists(std::string)` with `removeFileIfExists(NuXFiles::Path)`.
    - [ ] Replace `fileExists(std::string)` with `fileExists(NuXFiles::Path)`.
    - [ ] Replace `writeRasterToPng(std::string, ...)` and `loadPngRaster(std::string, ...)` with their Path overloads throughout tests.

  - Update small helpers and usages
    - [ ] Replace uses of `NativePath(entry.goldenPath)` etc. with direct Path arguments to Path-aware helpers; keep `NativePath` only where standard IO requires native strings (e.g., `std::ifstream`, `std::ofstream`).
    - [ ] Where a string literal path is used for fixture reads (e.g., expected text files), optionally build a Path via `SnapshotPathBridge::fromNative(literal)` for consistency, then stringify for std::ifstream.

  - Validation and grep sweep
    - [ ] Grep for remaining string-based helper calls in tests and migrate them:
      - `processFile(.*std::string)`
      - `logFileReport\(.*std::string` (Path-only in tests)
      - `ensureDirectory\(.*std::string` / `ensureParentDirectory\(.*std::string` / `removeFileIfExists\(.*std::string` / `fileExists\(.*std::string`
      - `buildSnapshotSourceTag\(.*std::string`
      - `writeRasterToPng\(.*std::string` / `loadPngRaster\(.*std::string`
    - [ ] Rebuild `TestSnapshotPlan` and run it; fix any missed conversions.

  - Cleanup after tests are Path-only
    - [ ] Remove string-only test adapters and helpers no longer used.
    - [ ] In production code, remove unused string-path overloads that were retained only for tests (e.g., `resolveSnapshotRoot(std::string, ...)`, `ensureParentDirectory(std::string)`, `renameFile(std::string, ...)`) once grep shows 0 references.
