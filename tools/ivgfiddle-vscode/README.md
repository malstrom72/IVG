# IVGFiddle VS Code Extension (Milestone 4 In Progress)

This folder contains the work-in-progress extension that embeds the IVGFiddle renderer inside Visual Studio Code. The initial milestone focuses on preparing the project structure, static assets, and helper scripts without introducing extra build dependencies. The bundled webview omits the ACE editor so the VS Code panel operates strictly as a preview surface that will render `.ivg` documents provided by the host extension logic.

## Folder Layout

- `src/` – TypeScript source for the extension entry point.
- `media/` – Static IVGFiddle assets copied from `tools/ivgfiddle/output/`.
- `scripts/` – Utility scripts for synchronizing the media folder with the upstream IVGFiddle build output.

## Building the Extension

1. Install dependencies locally (only TypeScript and VS Code types are required):

```bash
npm install
```

2. Compile the extension to produce `dist/extension.js`:

```bash
npm run compile
```

## Previewing `.ivg` Documents

1. Open or create a `.ivg` file in VS Code. The extension contributes a simple language configuration so the document is tagged with the `ivg` language ID automatically.
2. Run **IVGFiddle: Open** from the Command Palette. If the panel is already open it will be focused instead of spawning duplicates.
3. The preview canvas renders the active document after the WebAssembly runtime loads. When `ivgfiddle.preview.autoRefresh` is enabled (default), edits debounce according to `ivgfiddle.preview.debounceMs` (150 ms by default) before triggering a rerender.
4. Toggle auto refresh or adjust the debounce value under **Settings** → **Extensions** → **IVGFiddle Preview**. When auto refresh is disabled, the status bar switches to a clock icon and waits for a manual refresh.
5. Use **IVGFiddle: Refresh Preview** to force an immediate rerender of the active (or last previewed) IVG document.
6. Use **IVGFiddle: Clear Preview Trace** to reset the trace log without changing the rendered image.
7. Preview diagnostics from the webview surface inside VS Code—successful renders appear as transient status bar messages, while failures trigger error notifications.
8. A status bar entry titled `IVGFiddle Preview` shows the synchronized filename and the most recent render duration. It disappears whenever no IVG document is active and reappears when the panel regains focus.
9. Close the panel to stop synchronization; reopening the panel reuses the current editor contents.

## Updating IVGFiddle Assets

Whenever files under `tools/ivgfiddle/src/` change, rebuild and re-sync the assets before committing:

```bash
bash tools/ivgfiddle-vscode/scripts/sync-assets.sh --build
```

On Windows, run the accompanying script from any command prompt:

```cmd
scripts\sync-assets.cmd --build
```

Both scripts rebuild IVGFiddle when `--build` is supplied and mirror the fresh output into `media/`. Re-run them after every change to the IVGFiddle web app to keep the extension bundle up to date.
