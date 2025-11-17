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

- [ ] **Step 4 – Refactor core data structures to store `NuXFiles::Path`**
  - [x] Store `NuXFiles::Path` in `CommandLineOptions::includeDirs`, `fontDirs`, and `imageDirs`, keeping bridge-based conversions for CLI echoes and playback resolution.
  - [x] Persist the optional snapshot directory as `NuXFiles::Path` and retain a display mirror for user-facing text.
  - [ ] Convert `CommandLineOptions::ivgPaths` to `NuXFiles::Path` and update expansion helpers/tests that still expect raw UTF-8 strings.
  - [x] Have `SnapshotGolden` cache golden/draft/actual/diff/backup paths as `NuXFiles::Path`, stringifying only at filesystem/reporting boundaries.
  - [x] Store `NuXFiles::Path` inside `SnapshotEntryResult` for all per-entry artifacts, limiting bridge conversions to logging and serialization.
  - [ ] Thread `NuXFiles::Path` through `SnapshotRunResult` (including failure context) and any remaining per-entry metadata so runtime bookkeeping no longer persists native strings for filesystem objects.
  - [ ] Update `SharedResources` caches, playback scaffolding, and any helper structs that still receive string paths so they accept `NuXFiles::Path` inputs without redundant conversions.

- [ ] **Step 5 – Update filesystem helpers and execution paths**
  - Rewrite the helper layer (`fileExists`, `directoryExists`, `ensureDirectory*`, `removeFileIfExists`, `renameFile`, `collectDirectoryIvgFiles`, `expandInputPaths`) to operate on `NuXFiles::Path` arguments end-to-end.
  - Thread the new types through code that manipulates the filesystem—golden promotion (`SnapshotGolden`), diff creation, orphan audits, and CLI expansion—so no native strings are concatenated for path math.
  - Normalize caches and lookup tables that currently use string keys (image/font caches, processed base sets) so they compare canonical `Path` values and only stringify at the reporting boundary.

- [ ] **Step 6 – Adapt reporting, CLI parsing, and caching to the new types**
  - Wrap CLI argument ingestion in bridge helpers that immediately materialize `NuXFiles::Path` objects, retaining native strings only for diagnostics and compatibility with existing fixture text.
  - Adjust reporting utilities (`printScenarioListing`, `printEntryReport`, `formatErrorMessage`, `buildSnapshotSourceTag`, audit logs) to accept `Path` inputs and use a single conversion utility when they need human-readable output.
  - Audit any remaining caches keyed by string (sanitized identifiers, scenario labels) and document which ones must stay as strings after the core path migration.

- [ ] **Step 7 – Verify behavior and adjust tests**
  - Once the runtime compiles with `NuXFiles::Path` throughout, rebuild using `bash tools/IVGSnapshot/build-ivgsnapshot.sh` and the focused `./tools/BuildCpp.sh … TestSnapshotPlan` invocation.
  - Update `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` (and any golden fixtures) so expected logs list the new bridge-produced path strings, keeping list-only and audit output stable across platforms.
  - Capture any additional targeted commands required to validate the refactored path flow on both POSIX and Windows builds.

- [ ] **Step 8 – Retire legacy helper functions**
  - Remove or rewrite string-based utilities such as `buildSnapshotSourceTag`, `resolveRelativePath`, and path concatenation helpers once every caller consumes the new `SnapshotPathBridge` API.
  - Ensure the only remaining string concatenation for paths happens inside the bridge or reporting shims, preventing future regressions back to ad-hoc `+ "/" + component` patterns.

- [ ] **Step 9 – Clarify inline documentation**
  - After the refactor lands, refresh code comments around the migrated structs and helpers to explain where `NuXFiles::Path` lives and why string mirrors still exist.
  - Highlight any intentionally string-backed identifiers (scenario labels, sanitized IDs) so future work doesn’t accidentally convert them and break fixtures.

- [ ] **Step 10 – Update developer-facing references**
  - Revise `docs/IVGSnapshot.md` (and related developer notes) to describe the new path workflow, bridge helpers, and focused build commands once the refactor is merged.
  - Document any migration guidance for contributors (e.g., converting custom scripts or tests) that previously relied on string-based helpers that will be removed in Step 8.

## Risk mitigation and open questions
- Confirm NuX path behavior on Windows vs. POSIX to avoid breaking CLI inputs that previously relied on raw string normalization.
- Decide whether caches keyed by path should use `Path::equals` semantics or string normalization to avoid duplicate loads when the same file is referenced via different relative paths.
- Ensure error messages still surface human-readable paths (convert via `getFullPath()` or relative representation) and preserve existing wording to keep fixtures stable.
