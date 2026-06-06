# Include Asset Root Strategies for the VS Code Extension

## Current Behavior
The preview extension builds an include manifest by scanning every file that matches `**/*.{impd,ivg,ivgfont,png}` under the current workspace folders. It derives each bundle entry's mount path directly from `vscode.workspace.asRelativePath`, so any workspace that opens a subfolder ends up with mount paths that still include the leading project segments instead of the runtime root expected by `include` statements. 【F:tools/ivg-vscode/src/extension.ts†L1240-L1449】【F:tools/ivg-vscode/src/extension.ts†L1499-L1504】

IVGSnapshot already offers knobs such as `--root-dir` and `--include-dir` to control how paths are resolved when rendering snapshots. Mirroring that flexibility inside the extension would let developers preview the same layouts they validate with the command-line tooling. 【F:docs/IVGSnapshot.md†L40-L68】

## 1. Workspace Setting for Include Roots
**Summary.** Add a new setting like `ivgfiddle.includes.roots` that lists one or more folders (relative to the workspace or absolute) that should be treated as the include mount roots.

- **Extend `IncludeConfig`.** Load the list, normalize relative paths against the owning workspace, and surface validation errors in the existing include status UI. This keeps the configuration self-documenting and provides immediate feedback when a folder goes missing. 【F:tools/ivg-vscode/src/extension.ts†L47-L147】
- **Command palette integration.** Add an `IVGFiddle: Set Include Root…` command that shows a quick pick of all workspace folders plus their notable subdirectories (for example any folder containing `.ivg` files). Persist the chosen directory in `ivgfiddle.includes.roots`, update the status bar item to reflect the active root, and expose "Add Another"/"Clear" actions directly in the picker for rapid adjustments. 【F:tools/ivg-vscode/src/extension.ts†L971-L1631】
- **Watcher bootstrap.** When initializing watchers, convert each root entry into a `vscode.RelativePattern` so only the chosen trees are scanned instead of the entire workspace. Also listen for configuration changes and rebuild the watcher set immediately so the command palette flow updates the manifest without requiring a reload. 【F:tools/ivg-vscode/src/extension.ts†L1239-L1324】【F:tools/ivg-vscode/src/extension.ts†L1600-L1631】
- **Manifest build adjustments.** During manifest builds, update `getIncludeRelativePath` to strip the first matching root prefix before normalizing separators, falling back to the current behavior if no root matches. Record the last-used root alongside the manifest so future quick picks can highlight which entries are actively mapped. 【F:tools/ivg-vscode/src/extension.ts†L1427-L1504】

**Trade-offs.** Users have to populate the setting once per workspace, but the change is predictable and matches how other asset-heavy extensions expose include roots.

**Implementation Checklist.**
- [x] Add `ivgfiddle.includes.roots` to `package.json#contributes.configuration` with a default empty array, string array schema, and description that clarifies relative paths resolve against the workspace folder that owns the setting scope.
- [x] Extend the `IncludeConfig` load routine to read the new setting from `workspace.getConfiguration('ivgfiddle.includes')`, normalize each entry to a `Uri` tied to its workspace folder, filter out duplicates, and surface diagnostics via `configStatus` when a path cannot be resolved.
- [x] Update the include status bar item to show the active root count, expose `Set Include Root…`, `Add Another Root…`, and `Clear Roots` commands, and wire those commands to a new `setIncludeRoot` helper that accepts quick pick results.
- [x] Build a quick pick provider that lists all workspace folders and notable child directories containing `.ivg` or `.impd` files, supports manual folder browsing, and persists selections back to the configuration array while preserving existing roots unless explicitly cleared.
- [x] Convert watcher bootstrapping to iterate over the normalized root list, creating a `RelativePattern` per root so file system watchers scan only those trees; add a configuration change listener that rebuilds the watcher set when `ivgfiddle.includes.roots` mutates.
- [x] Adjust manifest construction so `getIncludeRelativePath` trims the first matching root prefix from each file path, records the matched root alongside the manifest entry, and falls back to workspace-relative paths when no configured root applies.
- [x] Cache the last-used roots in `workspaceState` to seed the command palette quick pick with recent selections and to highlight which roots were active during the last manifest rebuild.

## 2. Auto-Detection via Include Trace Feedback
**Summary.** Use the `[IVGFiddle] Include missing:` trace output to learn which mount paths the runtime expects, then search for them within the workspace to propose roots automatically.

**Implementation sketch.**
- Reuse the existing missing-include handler to collect candidate mount paths and debounce a background scan. 【F:tools/ivg-vscode/src/extension.ts†L1687-L1708】
- For each candidate, look for matching files by joining the path against every folder segment in the workspace; choose the deepest ancestor whose children satisfy the include. Once a stable mapping is found, persist it to the new `ivgfiddle.includes.roots` setting or cache it inside `globalState`.
- Present a quick pick that previews the detected mapping and lets the user accept or override the inferred root. The UI plumbing can hang off the existing include status bar item so no extra palette commands are required. 【F:tools/ivg-vscode/src/extension.ts†L1600-L1631】

**Trade-offs.** Heuristics might guess wrong for projects that intentionally reuse the same filename under different roots, so the command should always show the proposed mapping before enabling it.

## 3. Project Manifest File
**Summary.** Support a repository-scoped metadata file (for example `.ivgfiddle.json`) that declares include roots, additional search directories, and other preview defaults.

**Implementation sketch.**
- When activation runs, search upward from the workspace folder for the manifest file. Parse it once and merge its directives into the config object before watcher initialization. 【F:tools/ivg-vscode/src/extension.ts†L157-L188】【F:tools/ivg-vscode/src/extension.ts†L1239-L1324】
- Accept fields that mirror the IVGSnapshot command-line switches (`rootDir`, `includeDirs`, etc.) so teams can keep editor previews and CI renders in sync. 【F:docs/IVGSnapshot.md†L60-L68】
- Store the parsed manifest inside the extension context so manual rescans (`ivgfiddle.includes.rescan`) can reapply the same configuration without re-reading the file on every change. 【F:tools/ivg-vscode/src/extension.ts†L270-L275】

**Trade-offs.** Repositories need to commit another file, but the manifest eliminates per-user setup and keeps project defaults version-controlled.

## 4. Root Inference from Open Documents
**Summary.** Guess the include root by walking up from the active IVG file until the relative path to the document matches the mount path that IVGSnapshot would compute for snapshots.

**Implementation sketch.**
- Reuse `buildSnapshotSourceTag` logic from IVGSnapshot: once a candidate root yields the same normalized fragment as the runtime would produce, treat that folder as the include root. 【F:tools/IVGSnapshot/IVGSnapshot.cpp†L1058-L1145】
- Cache the chosen folder per workspace folder and update watchers plus `getIncludeRelativePath` accordingly. 【F:tools/ivg-vscode/src/extension.ts†L1239-L1504】
- Expose a command (`IVGFiddle: Adopt Snapshot Root`) that shows the inferred folder and lets users promote it into the persistent config or discard it if the guess is wrong.

**Trade-offs.** Works best when snapshot-enabled projects already normalize their structure; heterogeneous layouts might still require manual overrides.

## 5. Interactive Mount Path Remapping
**Summary.** Allow users to right-click an include entry in the explorer and choose "Remap Mount Path…" to point the bundle at a different on-disk location.

**Implementation sketch.**
- Extend the tree provider entries to keep track of the resolved `Uri` alongside the mount path so commands can retarget them. 【F:tools/ivg-vscode/src/extension.ts†L584-L789】
- When a remap is requested, prompt for the replacement file, compute its relative path, and update an in-memory override map that `getIncludeRelativePath` consults before falling back to workspace-relative values. 【F:tools/ivg-vscode/src/extension.ts†L1499-L1534】
- Persist overrides in `workspaceState` so they survive reloads but remain workspace-scoped, then surface a reset action in the include status menu for cleanup. 【F:tools/ivg-vscode/src/extension.ts†L971-L1631】

**Trade-offs.** This approach gives power users a precise tool when automation fails but requires extra UI affordances to keep overrides discoverable.
