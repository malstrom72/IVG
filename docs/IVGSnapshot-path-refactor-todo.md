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
  - Replaced `CommandLineOptions` directory/vector members with `NuXFiles::Path` containers backed by helper accessors so CLI echoing still emits UTF-8 strings without duplicating conversion logic.
  - Migrated `SnapshotEntryResult`/`SnapshotRunResult` to persist canonical `NuXFiles::Path` members for every filesystem artifact while keeping narrow-string mirrors exclusively for logs and JSON serialization.
  - Routed `SnapshotGolden`/`SnapshotPlaybackExecutor` through the new path containers, ensuring caches and identifier builders derive their string keys from sanitized `Path` data instead of manual concatenation.

- [x] **Step 5 – Update filesystem helpers and execution paths**
  - Promoted every internal filesystem helper (`ensureDirectory*`, `removeFileIfExists`, `renameFile`, diff/audit walkers) to operate on `NuXFiles::Path`, delegating directly to NuX primitives while keeping legacy error handling semantics.
  - Updated all call sites to pass `Path` objects end-to-end, eliminating redundant native-string conversions and centralizing the few remaining string fallbacks inside `SnapshotPathBridge`.
  - Normalized resource cache keys so playback/font/image lookups rely on canonical `Path` comparisons, documenting the handful of sanitized-string keys that remain for ASCII-only identifiers.

- [x] **Step 6 – Adapt reporting, CLI parsing, and caching to the new types**
  - Converted CLI ingestion to wrap every path argument in `NuXFiles::Path` immediately, stashing display strings only where diagnostics require native echoes.
  - Reworked reporting utilities (`printScenarioListing`, `formatErrorMessage`, `buildSnapshotSourceTag`, audit summaries) to consume `Path` objects and defer stringification to `SnapshotPathBridge` so fixtures retain their expected wording.
  - Audited caches/lookup maps to operate on canonical `Path` instances, documenting the few intentional string-key conversions that remain for sanitized identifiers.

- [x] **Step 7 – Verify behavior and adjust tests**
  - Expanded the IVGSnapshot verification flow to rebuild the tool with `bash tools/IVGSnapshot/build-ivgsnapshot.sh`, keeping the scope limited to the snapshot utility instead of the full `./build.sh` matrix.
  - Recompiled `tools/IVGSnapshot/tests/TestSnapshotPlan.cpp` against the `NuXFiles::Path`-aware code and re-ran the binary to confirm list-only output and diff logic still match the recorded fixtures.
  - Captured the new build command in this checklist so future runs pair `build-ivgsnapshot.sh` with the targeted `./tools/BuildCpp.sh … TestSnapshotPlan` invocation when validating CLI, playback, and audit paths.

- [x] **Step 8 – Retire legacy helper functions**
  - Replaced the remaining string-based tagging logic in `buildSnapshotSourceTag` with `SnapshotPathBridge::toNative` so canonical `NuXFiles::Path` math drives scenario naming before falling back to sanitized strings.
  - Audited the code for leftover bespoke path concatenation and wired the logging/reporting helpers through the bridge to avoid direct `+ "/" +` assembly.
  - Ensured the snapshot tool and its tests now share the same bridge helpers for conversions, allowing removal of the last ad-hoc path utilities from the refactor plan.

- [x] **Step 9 – Clarify inline documentation**
  - Added inline comments near `CommandLineOptions`, `SnapshotPathBridge`, `buildSnapshotSourceTag`, and `printScenarioListing` to describe how canonical paths propagate through the runtime and where sanitized string mirrors remain on purpose.
  - Confirmed the surrounding commentary now points contributors to the shared bridge instead of implying raw string concatenation or ad-hoc helpers.
  - Highlighted in-code boundaries between reporting and filesystem operations so future edits keep string fallbacks limited to user-facing output.

- [x] **Step 10 – Update developer-facing references**
  - Documented the canonical `NuXFiles::Path` lifecycle and bridge helpers in `docs/IVGSnapshot.md`, including guidance on how reporting derives sanitized strings.
  - Called out the dedicated IVGSnapshot build scripts so contributors can iterate without rebuilding the full repository, aligning the docs with the updated workflow.
  - Verified external-facing text no longer references the removed string helpers and instead directs readers to the new bridge-centric architecture.

## Risk mitigation and open questions
- Confirm NuX path behavior on Windows vs. POSIX to avoid breaking CLI inputs that previously relied on raw string normalization.
- Decide whether caches keyed by path should use `Path::equals` semantics or string normalization to avoid duplicate loads when the same file is referenced via different relative paths.
- Ensure error messages still surface human-readable paths (convert via `getFullPath()` or relative representation) and preserve existing wording to keep fixtures stable.
