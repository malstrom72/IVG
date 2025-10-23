# VS Code extension–synchronized resources TODO

Each milestone finishes with a verification run: execute the targeted checks listed under that milestone and close with the full repository build/test sweep (`timeout 600 ./build.sh`).

## Milestone 1 – Workspace discovery and watchers
- [x] Audit existing activation flow in `extension.ts` to confirm the best hook for registering include watchers.
- [x] Define glob patterns for include-eligible assets (`**/*.{impd,ivg,ivgfont,png}`) and document rationale.
- [x] Implement a `FileSystemWatcher` wiring that shares the debounce queue used by `scheduleDocument`.
- [x] Add telemetry hooks to log watcher churn and file counts for observability.
- [x] Run milestone smoke tests (`npm run compile`, `npm run lint`, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: open an `.ivg` document, toggle the include watcher setting, and confirm the status bar indicates watcher attachment without requiring a reload.

## Milestone 2 – Manifest generation and caching
- [ ] Design the `include-manifest.json` schema (mount path, byte length, checksum, MIME type).
- [ ] Implement manifest assembly using shared `workspace.fs.readFile` helpers and debounce alongside `syncDocument`.
- [ ] Persist cached bundles and manifests under `globalStorageUri`, pruning stale revisions.
- [ ] Surface manifest build progress through status-bar messaging and trace channels.
- [ ] Run milestone smoke tests (`npm run compile`, `npm run lint`, `node tools/ivgfiddle/testIVGFiddle.js`, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: trigger edits to multiple include files in quick succession and verify the status bar reports manifest rebuild progress while the include tree view (if available) updates without manual refresh.

## Milestone 3 – Upload handshake and messaging
- [ ] Define the `{ type: 'setIncludeBundle' }` payload contract shared by extension and webview.
- [ ] Implement REST client helpers for requesting presigned URLs or bundle IDs.
- [ ] Integrate bundle upload scheduling with `queueMessage` / `postMessageToWebview` throttle logic.
- [ ] Extend trace/status handling to record upload success, retries, and latency.
- [ ] Run milestone smoke tests (`npm run compile`, `npm run lint`, `node tools/ivgfiddle/testIVGFiddle.js`, targeted backend API unit tests, `timeout 600 ./build.sh`).
- [ ] VS Code user acceptance: modify an include asset, observe the upload progress UI, and confirm the preview refresh waits for the include bundle acknowledgment before rendering.

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
