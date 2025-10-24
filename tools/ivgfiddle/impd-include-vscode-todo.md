# VS Code extension–synchronized resources TODO

Each milestone finishes with a verification run: execute the targeted checks listed under that milestone and close with the full repository build/test sweep (`timeout 600 ./build.sh`).

## Milestone 1 – Workspace discovery and watchers

- [x] Audit existing activation flow in `extension.ts` to confirm the best hook for registering include watchers.
- [x] Define glob patterns for include-eligible assets (`**/*.{impd,ivg,ivgfont,png}`) and document rationale.
- [x] Implement a `FileSystemWatcher` wiring that shares the debounce queue used by `scheduleDocument`.
- [x] Add telemetry hooks to log watcher churn and file counts for observability.
- [x] Run milestone smoke tests (`npm run compile`, `npm run lint`, `timeout 600 ./build.sh`).
- [x] VS Code user acceptance: open an `.ivg` document, toggle the include watcher setting, and confirm the status bar indicates watcher attachment without requiring a reload.

## Milestone 2 – Manifest generation and caching

- [x] Design the `include-manifest.json` schema (mount path, byte length, checksum, MIME type).
- [x] Implement manifest assembly using shared `workspace.fs.readFile` helpers and debounce alongside `syncDocument`.
- [x] Persist cached bundles and manifests under `globalStorageUri`, pruning stale revisions.
- [x] Surface manifest build progress through status-bar messaging and trace channels.
- [x] Add a guarded `ivgfiddle.includes.manifestEnabled` setting so manifest work can be toggled without regressing baseline preview responsiveness.
- [x] Run milestone smoke tests (`npm run compile`, `npm run lint`, `node tools/ivgfiddle/testIVGFiddle.js`, `timeout 600 ./build.sh`).
- [x] VS Code user acceptance: validate manifest progress messaging and baseline preview responsiveness.
- [x] Preparation: open an IVG project with at least two include-eligible assets (for example, an `.impd` layout that references separate `.ivg` snippets or textures) so you can edit multiple files that influence the active preview.
- [x] Reminder: if no workspace or folder is open—or if the workspace only contains virtual (non-file) folders—the status bar shows `includes unavailable`; attach a file-backed workspace before validating the manifest flow.
- [x] Enable manifests: open **Settings > Extensions > IVG Preview** (or run "Preferences: Open Settings (UI)"), search for "include", check **IVGFiddle › Includes: Manifest Enabled**, and watch the status bar suffix progress from `includes pending` to `includes building`/`includes ready (...)` once the initial manifest completes (with `includes unavailable` persisting only when no workspace is attached).
- [x] Exercise manifest rebuilds: with the manifest toggle still on, make several quick edits (save or auto-save) across different include assets. Watch the status bar change to `includes building` during the burst and settle on `includes ready (...)` when the debounce completes. Hover the status bar item and confirm the tooltip reports `Include manifest status: building` during the rebuild and `Include manifest ready — …` afterward.
- [x] Disable manifests: return to settings, uncheck **IVGFiddle › Includes: Manifest Enabled**, and verify the status bar suffix switches back to `includes watching` (or `includes watching (+N)` once events arrive) while the tooltip notes "Include manifest disabled".
- [x] Verify preview responsiveness: with manifests disabled, edit the same include files again and confirm the IVG preview refreshes immediately after each save (no prolonged `includes building` state) while the status bar continues to show watcher activity counts incrementing.

## Milestone 3 – Upload handshake and messaging

- [x] Define the `{ type: 'setIncludeBundle' }` payload contract shared by extension and webview.
- [x] Implement REST client helpers for requesting presigned URLs or bundle IDs.
- [x] Integrate bundle upload scheduling with `queueMessage` / `postMessageToWebview` throttle logic.
- [x] Extend trace/status handling to record upload success, retries, and latency.
- [x] Run milestone smoke tests (`npm run compile`, `npm run lint`, `node tools/ivgfiddle/testIVGFiddle.js`, targeted backend API unit tests, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance:
    - Configure **IVGFiddle › Includes: Service Base Url** with a staging endpoint (or a placeholder such as `https://example.invalid` for a dry run) and enable manifest generation and uploads.
    - Edit multiple include files to watch the status bar transition through `includes building`, `includes ready • upload pending`, and `includes ready • bundle ready`, confirming the tooltip reports upload latency and bundle IDs.
    - Clear the service URL to confirm the status shifts to `upload disabled` while preview updates still arrive immediately.

## Milestone 4 – Editor UX contributions

- [ ] Add commands (e.g., `ivgfiddle.includes.rescan`) and settings mirroring existing preview preferences.
- [ ] Implement a `TreeDataProvider` that lists synchronized assets with context-menu actions.
- [ ] Provide command palette entries and status bar affordances for include sync health.
- [ ] Write UX copy and localization strings for new commands and views.
- [ ] Run milestone smoke tests (`npm run compile`, `npm run lint`, UX regression checklist, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: navigate the include explorer, perform context-menu actions (rescan, reveal in finder), and ensure settings toggles update behavior without restarting VS Code.

## Milestone 5 – IVGFiddle/webview/runtime integration

- [ ] Update the webview script to consume `setIncludeBundle` messages and mount bundles read-only.
- [ ] Adapt the sandbox loader to resolve `include` requests against the mounted archive.
- [ ] Implement cache invalidation to swap bundles atomically between preview runs.
- [ ] Add user-facing error messages when includes fail to mount or resolve.
- [ ] Run milestone smoke tests (`npm run compile`, `npm run lint`, `node tools/ivgfiddle/testIVGFiddle.js`, browser-based include resolution checks, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: run a preview session, change an include, and verify the browser preview reloads with the new content while older sessions warn about stale bundles.

## Milestone 6 – Backend enablement

- [ ] Design API surface for issuing presigned upload URLs and retrieving bundle manifests.
- [ ] Implement storage persistence with retention policies and project scoping.
- [ ] Add authentication/authorization checks to protect include assets.
- [ ] Provide integration tests covering upload, download, and manifest retrieval flows.
- [ ] Run milestone smoke tests (backend unit/integration suite, load tests for bundle uploads, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: using a staging backend, upload a new include via the extension and confirm the asset appears in server-side logs/audit tools with the correct project scope.

## Milestone 7 – Build, testing, and rollout

- [ ] Extend automated tests for the VS Code extension (unit/integration) to cover watchers, manifest churn, and messaging.
- [ ] Add webview/runtime tests verifying bundle mounting, cache invalidation, and include resolution.
- [ ] Integrate backend endpoint tests into CI to prevent regressions.
- [ ] Create a dogfood rollout plan with staged feature flags and rollback procedures.
- [ ] Update documentation and changelog entries summarizing the include-sync system.
- [ ] Run milestone smoke tests (full automation battery below plus `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: coordinate a dogfood session, gather qualitative feedback from extension users, and validate that rollout and rollback toggles behave as documented.

## Testing playbook

- [ ] VS Code extension TypeScript compilation: `cd tools/ivg-vscode && npm install && npm run compile && cd -`.
- [ ] VS Code extension linting pass: `cd tools/ivg-vscode && npm run lint && cd -`.
- [ ] Webview/runtime smoke tests: `node tools/ivgfiddle/testIVGFiddle.js` (optionally point to the built output directory).
- [ ] Backend API verification: execute the backend unit and integration test suite appropriate for the deployment target, including presigned URL issuance and bundle persistence cases.
- [ ] Full repository verification: `timeout 600 ./build.sh`.
