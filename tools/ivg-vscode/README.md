# IVG Preview VS Code Extension

The IVG Preview VS Code extension hosts the IVGFiddle renderer inside a webview so you can preview `.ivg` files directly in the editor without introducing heavy dependencies. The workspace intentionally sticks to built-in Node.js tooling—TypeScript and the VS Code API typings are the only packages installed via `npm`.

## Commands and Context Menus

- **IVG Preview: Open Preview** – opens or focuses the preview panel.
- **IVG Preview: Refresh Preview** – forces an immediate rerender of the active or last previewed IVG document.
- **IVG Preview: Clear Trace** – clears the runtime trace log without changing the rendered canvas.

All commands live under the **IVG Preview** category in the Command Palette.

The same **Open IVG Preview Panel** action also appears in:

- The Explorer context menu when you right-click a `.ivg` file.
- The editor tab context menu for active `.ivg` documents.

Both entries simply invoke `IVG Preview: Open Preview`, providing a quicker path to the preview without searching the palette.

## Configuration

Every setting is optional and exposed through the standard VS Code Settings UI:

- `ivgfiddle.syncOnOpen` (default `true`) – push the active IVG document to the webview whenever the panel opens.
- `ivgfiddle.webviewUpdateDelay` (default `0`) – delay, in milliseconds, before host messages reach the webview (useful when the renderer needs time to settle after activation).
- `ivgfiddle.preview.autoRefresh` (default `true`) – automatically refresh the preview when documents change.
- `ivgfiddle.preview.debounceMs` (default `150`) – wait time before sending auto-refresh updates when `autoRefresh` is enabled.

A reusable snippet named **IVG Canvas Skeleton** is available under the `ivg` language to quickly scaffold test documents.

## Project Layout

- `src/` – TypeScript sources for the extension entry point.
- `dist/` – compiled JavaScript emitted by `npm run compile`.
- `media/` – bundled IVGFiddle assets served inside the webview.
- `snippets/` – language snippets contributed to VS Code.
- `scripts/` – helper scripts for asset synchronization and packaging.

## Install from the SDK

The IVG SDK includes a prebuilt extension package:

```text
tools/ivg-vscode/ivg-vscode-0.0.1.vsix
```

Install it from the command line:

```bash
code --install-extension tools/ivg-vscode/ivg-vscode-0.0.1.vsix
```

Or install it inside VS Code:

1. Open the **Extensions** view.
2. Open the `...` menu.
3. Choose **Install from VSIX...**.
4. Select `tools/ivg-vscode/ivg-vscode-0.0.1.vsix` from the SDK checkout.

After installation, open an `.ivg` file and run **IVG Preview: Open Preview** from the Command
Palette or the `.ivg` file context menu. Snapshot-aware files show a **Snapshot** dropdown in the
preview toolbar so you can switch between `meta snapshot` scenarios.

## Local Build for Extension Development

1. Open a terminal in the repository root and switch to the extension folder:
   ```bash
   cd tools/ivg-vscode
   ```
2. Install the lightweight dev dependencies:
   ```bash
   npm install
   ```
3. Compile the extension:
   ```bash
   npm run compile
   ```
4. (Optional) Run the type-check-only lint gate:
   ```bash
   npm run lint
   ```

## Extension Development Host Walkthrough

1. From `tools/ivg-vscode`, launch VS Code and press **F5** (or use **Run and Debug → Start Debugging**) to open an Extension Development Host.
2. In the dev host, open or create a `.ivg` document. The language ID automatically resolves to `IVG`.
3. Trigger **IVG Preview: Open Preview** from the Command Palette or a context menu. The preview panel appears with the toolbar, canvas, and trace log.
4. Confirm the canvas updates as you edit the document (subject to the `autoRefresh` and `debounceMs` settings) and that notifications surface in the VS Code status bar.
5. Toggle `IVG Preview` settings to observe manual-refresh and deferred-sync behaviour, then clear the trace log using its command.
6. Close the preview to verify the status bar indicator hides automatically, then reopen the panel to ensure synchronization resumes (or remains deferred if `syncOnOpen` is disabled).

## Packaging

Use the portable packaging helper to generate a `.vsix` without a global `vsce` install. It rebuilds IVGFiddle and syncs the assets first:

```bash
bash scripts/package.sh
```

The script checks for `vsce` on your `PATH` and falls back to `npx vsce package` when necessary. The resulting archive appears in the extension folder.

### Manual Packaging or Development Alternatives

- Package directly with `npx`:
  ```bash
  npx vsce package
  ```
- Install a package built in this folder:
  ```bash
  code --install-extension ivg-vscode-0.0.1.vsix
  ```
- Run from source without packaging:
  ```bash
  code --extensionDevelopmentPath "$(pwd)"
  ```

## Updating IVG Preview Assets

When assets under `tools/ivgfiddle/` change, rebuild and mirror the output into the extension:

```bash
bash tools/ivg-vscode/scripts/sync-assets.sh --build
```

On Windows, use the companion script:

```cmd
scripts\sync-assets.cmd --build
```

Both scripts rebuild IVGFiddle when `--build` is supplied and copy the generated rasterizer into `media/`. The extension-specific
`setupModule.js` loader remains untouched so the VS Code preview handshake stays intact.

## Capturing Screenshots or GIFs

Follow the step-by-step capture notes in [`media/docs/capture-preview.md`](media/docs/capture-preview.md) when you need visual verification for release notes or manual testing.

## Troubleshooting

- **TypeScript cannot find the `vscode` module** – run `npm install` to restore the bundled type declarations.
- **Webview fails to update immediately after opening** – increase `ivgfiddle.webviewUpdateDelay` or disable `ivgfiddle.syncOnOpen` and trigger a manual refresh once the renderer is ready.
- **Preview trace still shows prior logs** – run **IVG Preview: Clear Trace**; the canvas remains unchanged.
- **Lint failures** – run `npm run lint` and address any reported issues before packaging.

For additional verification checklists, consult `docs/ivg-vscode-extension-plan.md` in the repository root.
