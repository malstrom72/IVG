# Handling `include` in IMPD/IVG Without Random File Access

IVGFiddle runs the IMPD toolchain in a sandboxed browser/WebAssembly environment. That sandbox deliberately blocks direct disk I/O: the runtime can only touch files that are explicitly uploaded, synced from the companion VS Code extension, or streamed from the backend. Allowing an `include` directive to wander the user's filesystem would break the security model (it would require elevated privileges in the browser, violate the hosted service's isolation guarantees, and bypass the deterministic build inputs IVGFiddle depends on). As a result, ad-hoc reads like `include "../../secret.cfg"` are impossible.

What *is* allowed today are packaged or streamed assets: IVGFiddle can load blobs that the user uploads through its UI, resources synchronized via the VS Code extension, or data served by project-scoped APIs. Everything must be registered ahead of time so the sandboxed runtime can mount it inside its virtual filesystem.

## VS Code extension–synchronized resources

The IVG VS Code extension already owns the event loop for `.ivg` documents and holds the plumbing we need for deterministic include sync.
Its entry point (`activate` in `extension.ts`) wires document-open and document-change listeners.【F:tools/ivg-vscode/src/extension.ts†L31-L157】
It keeps track of the last previewed document so the status bar and refresh logic stay accurate.【F:tools/ivg-vscode/src/extension.ts†L157-L215】
The same activation routine funnels edits into the preview webview via `queueMessage` and `postMessageToWebview`, ensuring a dedicated channel we can reuse for include manifests.【F:tools/ivg-vscode/src/extension.ts†L185-L275】
Building on that infrastructure we can layer a richer include workflow that respects sandbox boundaries yet feels native inside the editor.

- **Workspace discovery and watchers.**
The extension can register a `FileSystemWatcher` scoped to include-friendly resources.
Starter glob patterns such as `**/*.{impd,ivg,ivgfont,png}` cover both source snippets and asset files.
The watcher should activate alongside `onDidOpenTextDocument` and `onDidChangeTextDocument` so include edits flow through the same scheduling pipeline as `.ivg` documents.【F:tools/ivg-vscode/src/extension.ts†L31-L157】
`workspace.fs.stat` and `workspace.fs.readFile` already power `getWebviewContent`, allowing the watcher to reuse those metadata loaders when packaging include assets.【F:tools/ivg-vscode/src/extension.ts†L119-L185】
Those shared code paths inherit the async/await error handling that feeds into the trace output and status messaging system, keeping telemetry consistent with today's preview updates.【F:tools/ivg-vscode/src/extension.ts†L276-L404】
- **Manifest generation and caching.**
A debounced background job—mirroring the cadence of `scheduleDocument`—can assemble an `include-manifest.json` that records each synchronized asset's mount path, byte length, and checksum.【F:tools/ivg-vscode/src/extension.ts†L185-L275】
By running next to `syncDocument`, the manifest builder can surface progress through the existing status-bar UX, including duration readouts and manual-refresh hints driven by `showStatusBar`.【F:tools/ivg-vscode/src/extension.ts†L215-L360】
Collected files stream into an archive under the extension's `globalStorageUri`, and uploads trigger only when checksum deltas appear so local edits stay snappy.
- **Upload handshake.**
The extension already serializes IVG source into the webview by queueing `setSource` payloads and sending them through `postMessageToWebview` (honoring any configured `webviewUpdateDelay`).【F:tools/ivg-vscode/src/extension.ts†L185-L275】
We can introduce a sibling payload such as `{ type: 'setIncludeBundle', manifest, presignedUrl }` that advertises the packaged includes before the next preview render.
A lightweight REST helper can request presigned URLs or bundle identifiers, while the existing message throttle keeps large transfers from overwhelming the UI.
The webview already emits `'status'` and `'trace'` packets that flow through `processStatusMessage` and `processTraceMessage`, so include uploads inherit acknowledgment, error reporting, and timing hooks for free.【F:tools/ivg-vscode/src/extension.ts†L51-L157】【F:tools/ivg-vscode/src/extension.ts†L360-L475】
- **Editor-facing commands and UI.**
Package metadata can surface through new VS Code contributions that sit beside `ivgfiddle.open`, `ivgfiddle.refreshPreview`, and `ivgfiddle.clearTrace` in the manifest.【F:tools/ivg-vscode/package.json†L1-L86】
A dedicated `TreeDataProvider` (for example a view ID of `ivgfiddle.includes`) would let authors browse synchronized assets, trigger context-menu uploads, and pin frequently referenced files.
Follow-up commands such as "Rescan Includes" or "Attach Folder as Include Root" can reuse the existing command registration pattern, while settings like `ivgfiddle.includes.autoSync` mirror the structure of `ivgfiddle.preview.autoRefresh` for discoverability.【F:tools/ivg-vscode/package.json†L1-L86】
- **Division of labor between VS Code and IVGFiddle.**
Once a bundle is published, IVGFiddle only receives immutable manifests and blob handles, which mount read-only inside the WebAssembly sandbox.
The extension remains the sole authority for edits, leveraging its filesystem access while respecting the hosted environment's security boundaries.
Before dispatching the next `setSource` message, the extension can confirm that the include archive is current so the browser preview and the workspace stay perfectly aligned.

### Implementation complexity estimate

- **VS Code extension work.**
	- *File watching and manifest build pipeline:* ~250–350 SLOC to add watcher registration, debounce helpers, manifest builders, and checksum caching. Expect 1.5–2.5 engineering days (or ~2–3 focused Codex coding sessions) given the need to thread async error handling through `queueMessage`, `syncDocument`, and the shared status infrastructure.【F:tools/ivg-vscode/src/extension.ts†L31-L475】
	- *Upload transport and message wiring:* ~150–220 SLOC to define REST helpers, payload schemas, and webview message handlers alongside the existing `setSource` pathway. Budget roughly 1 engineering day (1–2 Codex sessions) to iterate on the handshake and throttle tuning.【F:tools/ivg-vscode/src/extension.ts†L185-L360】
	- *Editor UI surfaces (tree view, commands, settings):* ~200–280 SLOC across `extension.ts`, `package.json`, and new view/command modules. Another 1–1.5 engineering days for Codex to scaffold the `TreeDataProvider`, register commands, and localize strings.【F:tools/ivg-vscode/src/extension.ts†L215-L360】【F:tools/ivg-vscode/package.json†L1-L86】
- **IVGFiddle/webview integration.**
	- Handling the `{ type: 'setIncludeBundle' }` payload, mounting bundles, and exposing include lookups inside the preview runtime likely costs 180–260 SLOC across the webview script and sandbox loader. Plan for 1–2 engineering days or ~2 Codex sessions because of the testing matrix between cached and freshly uploaded bundles.
- **Backend support.**
        - Presigned URL issuance, bundle persistence, and snapshot APIs typically add 220–320 SLOC (controller, routing, storage utility, and tests). Another 1.5 engineering days—or ~2–3 Codex sessions—to settle authentication and retention policies.
- **Total effort.**
        - Aggregating the above, anticipate 800–1,200 new or modified SLOC. A single developer (or Codex-assisted workflow) should expect roughly 5–7 engineering days end-to-end, including review cycles, integration testing, and UX polish.

Every milestone in the accompanying TODO plan should close with a full repository build (`timeout 600 ./build.sh`) plus the milestone-specific smoke tests listed there so regressions surface immediately.

A sustainable answer for IMPD `include` support must therefore avoid bespoke rebuilds every time a new dependency appears. The solutions below focus on persistent systems—often with a small GUI surface—that let authors manage include assets once and then reuse them inside both IVGFiddle and the VS Code extension.

## 1. Workspace include library with drag-and-drop management

- Extend IVGFiddle with a side panel that lists all includeable resources for the current workspace.
- Users drag files from their desktop (or drop URLs) and the panel stores them inside a versioned, per-project package.
- The panel exposes quick actions to rename, delete, and preview resources so the include graph stays tidy without leaving the browser.
- On save, IVGFiddle emits an updated virtual package (e.g. a zipped bundle or IndexedDB payload). The runtime resolves `include` lookups straight from that package, so new assets are instantly available without rebuilding the application.
- The VS Code plugin mirrors the same package schema: the extension syncs the resource list, shows the same include browser, and uploads deltas to IVGFiddle when the user hits “Run”.

## 2. Declarative manifest editor with live validation

- Introduce an `includes.manifest.json` (or similar) that declares every importable asset along with metadata such as MIME type, size, and checksum.
- Provide a GUI editor in IVGFiddle/VS Code that keeps the manifest in sync with actual files, highlights missing references, and lets users reorganize folders visually.
- During preview, IVGFiddle streams the manifest and a single concatenated blob; a lightweight loader performs offset-based reads to satisfy `include` requests lazily.
- Because manifests are declarative and GUI-managed, adding a new include only involves updating the manifest entry—no rebuild scripts or manual packaging work.

## 3. Remote include service with credentialed browsing

- Host include assets in a dedicated object store (S3, GCS, etc.) behind a minimal API that supports list/upload/download operations scoped to a project.
- IVGFiddle exposes a file browser dialog (with thumbnail previews or metadata columns) that talks to the API using short-lived tokens.
- The VS Code plugin reuses the same API, so local edits synchronize automatically and include paths stay consistent between environments.
- The IMPD runtime swaps filesystem reads for signed HTTPS requests; responses are cached in memory so repeated includes are instant while still honoring sandbox limits.
- Updating or adding an include is simply an upload via the UI—no application rebuild necessary.

## 4. Snapshot-and-share include workspaces

- Offer a “Create Include Workspace” action in IVGFiddle that captures the current project state into a read-only snapshot hosted by the backend.
- The GUI displays snapshot history, diff views, and a “promote to active” button so teams can coordinate include updates without touching the base build.
- When a session starts, IVGFiddle mounts the chosen snapshot as an in-memory filesystem. The IMPD loader reads from the snapshot, guaranteeing stable include resolution while allowing quick swaps when new assets are published.
- The VS Code plugin receives the snapshot manifest and lets developers switch snapshots or stage a new one directly from the editor, ensuring both tools stay aligned without rebuilding IVGFiddle itself.

These patterns prioritize reusable infrastructure and user-facing controls, preventing ad-hoc rebuilds while keeping IMPD `include` fully functional inside the sandboxed tooling environment.
