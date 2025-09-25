# IVGFiddle VS Code Extension Implementation Plan

## Milestone 1: Prepare Project Structure and Assets
- [x] Create a minimal VS Code extension scaffold manually (no Yeoman) with `package.json`, `tsconfig.json`, and `src/extension.ts` using only built-in Node modules.
	- [x] Initialize the folder structure `ivgfiddle-vscode/{src,media,scripts}` and create a `.vscodeignore` that omits nothing but `scripts/`.
	- [x] Author `package.json` with explicit `engines.vscode`, `activationEvents: ["onCommand:ivgfiddle.open"]`, `main: "dist/extension.js"`, and scripts `"compile": "tsc -p ."`, `"watch": "tsc -w -p ."`.
	- [x] Write `tsconfig.json` targeting `es2020`, module `commonjs`, `rootDir: "src"`, `outDir: "dist"`, and enable `strict` mode to match VS Code samples without extra tooling.
	- [x] Seed `src/extension.ts` with boilerplate that registers the `ivgfiddle.open` command and logs activation for smoke testing.
- [x] Copy the existing IVGFiddle static assets (HTML, JS, CSS) into a `media/` directory within the extension, referencing them with relative paths.
	- [x] Run `bash tools/ivgfiddle/buildIVGFiddle.sh` if `tools/ivgfiddle/output/ivgfiddle.html` is missing to guarantee the bundle exists. (Existing bundle reused; rebuild script available via sync helpers.)
	- [x] Mirror the output directory to `media/` preserving subfolders (`ace/`, `assets/`, etc.) so that relative references inside `ivgfiddle.html` remain valid.
	- [x] Replace absolute CDN links in the HTML with local copies when feasible to keep the dependency footprint minimal. (No CDN references were present, so no rewrites were required.)
- [x] Document the asset copy process in a simple shell script (no dependencies) that syncs from `tools/ivgfiddle/output/` to the extension folder.
	- [x] Implement `scripts/sync-assets.sh` that `cd`’s to repo root, runs the build script when a `--build` flag is passed, then uses `rsync -a --delete` (portable alternative: `tar`/`cp -R`) without requiring non-standard binaries.
	- [x] Add a matching Windows `.cmd` script performing the same copy using `robocopy` or `xcopy` with built-in flags.
	- [x] Update the README to instruct contributors to re-run the sync script whenever IVGFiddle changes.
- [ ] **Tests:**
	- [x] Run `npm run compile` using only TypeScript compiler bundled via `devDependencies` and confirm `dist/extension.js` is emitted.
	- [ ] Launch the extension in VS Code’s Extension Development Host (`F5`) and trigger the `ivgfiddle.open` command, checking the developer console for the activation log line and the absence of missing file errors. *(Pending manual verification in the Extension Development Host.)*

## Milestone 2: Webview Integration with Static IVGFiddle
- [ ] Instantiate a Webview panel in `extension.ts` that loads `ivgfiddle.html` from the bundled assets using `asWebviewUri`.
	- [ ] Build a helper `getWebviewContent(webview: vscode.Webview)` that reads `media/ivgfiddle.html` from disk, then rewrites `src`/`href` attributes via regex to call `webview.asWebviewUri(vscode.Uri.joinPath(extensionUri, "media", path))`.
	- [ ] Store the panel instance in a module-level variable to keep a single shared view (focus if already open).
- [ ] Adjust the HTML to satisfy VS Code’s default Content Security Policy (move inline scripts/styles into separate files if necessary).
	- [ ] Extract inline `<script>` blocks to files under `media/scripts/` and reference them with `<script src="..." nonce="${nonce}"></script>` using a generated nonce in the extension code.
	- [ ] Replace inline styles with a dedicated CSS file or `webview.cspSource`-scoped `<style>` tag emitted from the extension.
- [ ] Ensure all asset URIs are rewritten to use Webview URIs without relying on additional build tooling.
	- [ ] Parse `ivgfiddle.html` for `src`, `href`, and `url(...)` references; for data attributes in JavaScript, replace string literals (e.g., `"./assets/foo.png"`) using a small transformation step executed when reading the file.
	- [ ] Provide a fallback that throws if a referenced asset is missing to surface issues early during activation.
- [ ] **Tests:**
	- [ ] Trigger the command inside the Extension Development Host and confirm the Webview renders IVGFiddle without console errors.
	- [ ] Manually interact with the IVGFiddle UI elements (editing IVG text, toggling controls) to confirm the UI behaves identically to the standalone page, including verifying that external network requests are absent.

## Milestone 3: Document Synchronization (One-Way)
- [ ] Use the `vscode.workspace.onDidOpenTextDocument` and `onDidChangeTextDocument` events to forward `.ivg` file contents into the Webview via `postMessage`.
	- [ ] Filter events to `document.languageId === "ivg"` (define a language contribution mapping `.ivg` to `ivg`).
	- [ ] When the panel becomes visible, send a `initialize` message carrying the active document URI, text, and version number.
	- [ ] On `onDidChangeTextDocument`, throttle emissions with `setTimeout` (e.g., 150 ms) to avoid flooding the Webview.
- [ ] Inside the Webview, listen for incoming messages and replace the IVG editor contents without requiring external libraries.
	- [ ] Register `window.addEventListener("message", handler)` that checks `message.type` and sets `aceEditor.getSession().setValue(payload.text)` only when versions advance.
	- [ ] Preserve scroll position by capturing `aceEditor.session.getScrollTop()` before replacing the text and restoring afterward.
- [ ] Provide a status bar item indicating when a document is synchronized.
	- [ ] Create a `vscode.StatusBarItem` aligned left with text like `$(sync) IVGFiddle: filename.ivg` updated on selection change.
	- [ ] Dispose of the item when the panel closes to avoid stale indicators.
- [ ] **Tests:**
	- [ ] Open a `.ivg` file in the Extension Development Host and confirm the Webview updates immediately (validate both newly created and existing files).
	- [ ] Modify the file and ensure the Webview reflects the changes without reloading while the status bar text updates to show the active filename.
	- [ ] Close the document to verify the status bar indicator clears and no further messages are sent (use Webview devtools to watch network traffic).

## Milestone 4: Two-Way Editing Support
- [ ] Emit `postMessage` events from the Webview when IVGFiddle content changes (e.g., ACE editor change event) and apply edits to the active VS Code document via `TextEditorEdit`.
	- [ ] Subscribe to ACE’s `session.on('change', handler)` and send `{ type: 'edit', text, version }` only when the change originated from the Webview (skip updates triggered by incoming host edits by tracking a `suppressNextChange` flag).
	- [ ] On the extension side, call `activeEditor.edit(editBuilder => editBuilder.replace(fullRange, text))` while recording the document version processed to prevent out-of-order application.
- [ ] Implement debounce logic using `setTimeout` to avoid feedback loops while still avoiding extra dependencies.
	- [ ] Wrap the outgoing Webview event in `setTimeout` with a configurable delay (default 150 ms) stored in `workspace.getConfiguration('ivgfiddle').get('webviewUpdateDelay')`.
	- [ ] Cancel pending timers when a new edit arrives to collapse rapid keystrokes into a single update.
- [ ] Guard against unsaved file states by warning users before overwriting dirty editors.
	- [ ] Before applying Webview edits, check `activeTextEditor.document.isDirty`; if true, show `vscode.window.showWarningMessage` with options to overwrite, save, or cancel.
	- [ ] Respect the user’s choice by skipping the incoming edit when they cancel and sending a `revert` message back to the Webview to resynchronize.
- [ ] **Tests:**
	- [ ] Edit within the Webview and confirm the VS Code document updates in near real-time, inspecting document version increments in the VS Code log output channel.
	- [ ] Edit within VS Code and ensure the Webview stays synchronized (verify no infinite loops by watching the console for repeated messages and by logging suppression flag transitions).
	- [ ] Toggle the dirty state by introducing unsaved changes and confirm the warning logic behaves correctly by choosing each action (overwrite, save, cancel) and observing the resulting state in both the editor and Webview.

## Milestone 5: Packaging, Configuration, and Documentation
- [ ] Add contribution points to `package.json` (command palette entry, activation events, basic configuration options) without pulling extra schemas.
	- [ ] Define `contributes.commands` with title `"Open IVGFiddle"` and category `"IVGFiddle"`, plus `contributes.menus.commandPalette` to expose it.
	- [ ] Register a JSON schema-free configuration section `ivgfiddle` with settings like `syncOnOpen` (boolean) and `webviewUpdateDelay` (number) to keep customization lightweight.
	- [ ] Include `contributes.languages` for `.ivg` files and `contributes.snippets` pointing to a manually-authored JSON snippet file for boilerplate IVG content.
- [ ] Write minimal documentation (`README.md`) detailing installation, commands, and dependency-free design.
	- [ ] Document prerequisites (Node.js, VS Code), the asset sync script usage, command behavior, and troubleshooting steps for CSP issues.
	- [ ] Provide GIF or screenshot instructions for verifying successful launch, stored under `media/docs/` and referenced relatively.
- [ ] Use `vsce` only for packaging; if unavailable, provide manual instructions for installing from source via `code --install-extension`.
	- [ ] Add `scripts/package.sh` that checks for `vsce` and falls back to `npx vsce package` (bundled with npm) so no global install is required.
	- [ ] Document manual installation: `code --install-extension ivgfiddle-vscode-0.0.1.vsix` and alternative `code --extensionDevelopmentPath` workflow.
- [ ] **Tests:**
	- [ ] Run `npm run lint` using TypeScript’s built-in diagnostics or a simple `tsc --noEmit` if linting is unnecessary, capturing the output in the README troubleshooting section.
	- [ ] Package the extension with `vsce package` and confirm the resulting `.vsix` installs and runs inside VS Code by opening a fresh Extension Development Host window.
	- [ ] Follow the README instructions from a clean environment (e.g., another workspace) to ensure they are accurate, keeping notes on any missing prerequisites and updating the doc accordingly.
